#include "lut-synth/synthesis/synthesis.hpp"
#include "lut-synth/synthesis/variable-selection.hpp"
#include "lut-synth/resynthesis/resynthesis-util.hpp"

#include <kitty/operations.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/views/topo_view.hpp>

#include <cassert>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <utility>
#include <vector>

namespace lut_synth {

namespace {

struct ReshapedTT {
    std::vector<kitty::dynamic_truth_table> tables;
};

ReshapedTT reshape(std::vector<kitty::dynamic_truth_table> const &tts, int k, int num_vars) {
    int const m = static_cast<int>(tts.size());
    int const remaining_vars = num_vars - k;
    int const num_cofactors = 1 << k;

    ReshapedTT reshaped;
    reshaped.tables.resize(num_cofactors * m, kitty::dynamic_truth_table(remaining_vars));

    ss_detail::for_each_cofactor_entry(k, num_vars, m, [&](int cof_idx, int assignment, int output, int original_index) {
        if (kitty::get_bit(tts[output], original_index)) {
            kitty::set_bit(reshaped.tables[cof_idx * m + output], assignment);
        }
    });

    return reshaped;
}

std::vector<mockturtle::xag_network::signal>
generate_selects(mockturtle::xag_network &network, ReshapedTT const &reshaped,
                 std::vector<mockturtle::xag_network::signal> const &pis) {
    std::vector<kitty::dynamic_truth_table> anfs;
    anfs.reserve(reshaped.tables.size());
    for (auto const &table : reshaped.tables) {
        anfs.push_back(ss_detail::to_anf(table));
    }
    return ss_detail::select_outputs_from_anfs(network, anfs, pis);
}

mockturtle::xag_network synthesize_for_k(std::vector<kitty::dynamic_truth_table> const &tts,
                                         int num_vars, int k) {
    assert(k >= 0 && k <= num_vars);

    const int num_outputs = static_cast<int>(tts.size());

    mockturtle::xag_network network;
    std::vector<mockturtle::xag_network::signal> pis = ss_detail::create_pis(network, num_vars);

    std::vector<mockturtle::xag_network::signal> select_pis(pis.begin(), pis.begin() + k);
    std::vector<mockturtle::xag_network::signal> remaining_pis(pis.begin() + k, pis.end());

    const ReshapedTT reshaped = reshape(tts, k, num_vars);
    const std::vector<mockturtle::xag_network::signal> select_outputs = generate_selects(network, reshaped, remaining_pis);

    ss_detail::build_mux_tree(network, select_outputs, select_pis, num_outputs, k);

    return mockturtle::cleanup_dangling(network);
}

bool verify(mockturtle::xag_network const &network,
            std::vector<kitty::dynamic_truth_table> const &tts) {
    if (network.num_pis() != tts.front().num_vars()) {
        return false;
    }
    const mockturtle::default_simulator<kitty::dynamic_truth_table> simulator(tts.front().num_vars());
    const std::vector<kitty::dynamic_truth_table> simulation = mockturtle::simulate<kitty::dynamic_truth_table>(network, simulator);
    if (simulation.size() != tts.size()) {
        return false;
    }
    for (size_t i = 0; i < tts.size(); ++i) {
        if (simulation[i].num_vars() != tts[i].num_vars()) {
            return false;
        }
        if (simulation[i] != tts[i]) {
            return false;
        }
    }
    return true;
}

std::pair<mockturtle::xag_network, uint32_t>
try_split(std::vector<kitty::dynamic_truth_table> const &tts, int k) {
    mockturtle::xag_network network = synthesize_for_k(tts, static_cast<int>(tts.front().num_vars()), k);
    if (!verify(network, tts)) {
        return {mockturtle::xag_network{}, std::numeric_limits<uint32_t>::max()};
    }
    uint32_t and_count = count_ands(network);
    return {std::move(network), and_count};
}

/* Rebuild ``permuted`` so its PI order matches the original (un-permuted)
 * variable order. ``permuted`` was synthesized from tts that had been
 * passed through ``permute_all_variables(tts, order)``; its PI_j computes
 * variable j of the permuted tts, which is variable ``order[j]`` of the
 * original tts. To undo this, route ``permuted``'s PI_j into the new
 * network's new_pi[order[j]]. Without this rewrite, multi-start synthesis
 * returns an XAG that evaluates the wrong function. */
mockturtle::xag_network rewire_pis_with_order(
    mockturtle::xag_network const &permuted,
    std::vector<uint32_t> const &order) {
    using Net = mockturtle::xag_network;
    using signal = Net::signal;
    using node = Net::node;

    Net result;
    uint32_t const num_pis = static_cast<uint32_t>(order.size());
    std::vector<signal> new_pis(num_pis);
    for (uint32_t i = 0; i < num_pis; ++i) {
        new_pis[i] = result.create_pi();
    }

    mockturtle::unordered_node_map<signal, Net> node_to_sig(permuted);
    node_to_sig[permuted.get_node(permuted.get_constant(false))] =
        result.get_constant(false);
    if (permuted.get_node(permuted.get_constant(true)) !=
        permuted.get_node(permuted.get_constant(false))) {
        node_to_sig[permuted.get_node(permuted.get_constant(true))] =
            result.get_constant(true);
    }

    uint32_t pi_idx = 0;
    permuted.foreach_pi([&](auto pi_node) {
        node_to_sig[pi_node] = new_pis[order[pi_idx]];
        ++pi_idx;
    });

    mockturtle::topo_view<Net> topo(permuted);
    topo.foreach_node([&](auto n) {
        if (permuted.is_constant(n) || permuted.is_pi(n)) return;
        std::vector<signal> children;
        permuted.foreach_fanin(n, [&](auto f) {
            signal c = node_to_sig[permuted.get_node(f)];
            if (permuted.is_complemented(f)) c = !c;
            children.push_back(c);
        });
        signal out;
        if (permuted.is_and(n)) {
            out = result.create_and(children[0], children[1]);
        } else {
            out = result.create_xor(children[0], children[1]);
        }
        node_to_sig[n] = out;
    });

    permuted.foreach_po([&](auto f) {
        signal c = node_to_sig[permuted.get_node(f)];
        if (permuted.is_complemented(f)) c = !c;
        result.create_po(c);
    });
    return mockturtle::cleanup_dangling(result);
}

} // namespace

mockturtle::xag_network synthesize_selectswap_internal(std::vector<kitty::dynamic_truth_table> const &tts,
                                                int num_vars, int k) {
    int best_k = k;

    if (k == -1) {
        mockturtle::xag_network best_network;
        uint32_t best_and = std::numeric_limits<uint32_t>::max();
        uint32_t best_size = std::numeric_limits<uint32_t>::max();

        for (int current_k = 1; current_k < num_vars; ++current_k) {
            auto [candidate, and_count] = try_split(tts, current_k);
            if (and_count == std::numeric_limits<uint32_t>::max()) {
                continue;
            }
            const uint32_t size = static_cast<uint32_t>(candidate.size());
            if (and_count < best_and || (and_count == best_and && size < best_size)) {
                best_and = and_count;
                best_k = current_k;
                best_size = size;
                best_network = std::move(candidate);
            }
        }

        assert(best_and != std::numeric_limits<uint32_t>::max());
        return best_network;
    }

    assert(k > 0 && k < num_vars);
    auto [network, and_count] = try_split(tts, best_k);
    assert(and_count != std::numeric_limits<uint32_t>::max());
    return network;
}

mockturtle::xag_network synthesize_selectswap(std::vector<kitty::dynamic_truth_table> const &tts, int k,
                                       uint32_t num_random_starts, uint64_t seed) {
    const uint32_t num_vars_checked = check_tts(tts, "synthesize_selectswap");
    int const num_vars = static_cast<int>(num_vars_checked);

    if (num_random_starts <= 1) {
        return synthesize_selectswap_internal(tts, num_vars, k);
    }

    mockturtle::xag_network best_network;
    uint32_t best_and = std::numeric_limits<uint32_t>::max();

    std::mt19937_64 rng(seed);
    std::vector<uint32_t> order(num_vars);
    std::iota(order.begin(), order.end(), 0);

    for (uint32_t trial = 0; trial < num_random_starts; ++trial) {
        std::shuffle(order.begin(), order.end(), rng);
        std::vector<kitty::dynamic_truth_table> permuted_tts = ss_detail::permute_all_variables(tts, order);
        mockturtle::xag_network candidate_perm =
            synthesize_selectswap_internal(permuted_tts, num_vars, k);
        mockturtle::xag_network candidate = rewire_pis_with_order(candidate_perm, order);
        uint32_t and_count = count_ands(candidate);

        if (and_count < best_and) {
            best_and = and_count;
            best_network = std::move(candidate);
        }
    }

    return best_network;
}

uint32_t count_ss_ands_for_k(std::vector<kitty::dynamic_truth_table> const &tts, int k) {
    auto [candidate, and_count] = try_split(tts, k);
    return and_count;
}

mockturtle::xag_network synthesize_selectswap_with_order(std::vector<kitty::dynamic_truth_table> const &tts,
                                                  std::vector<uint32_t> const &var_order, int k) {
    const uint32_t num_vars_checked = check_tts(tts, "synthesize_selectswap_with_order");
    int const num_vars = static_cast<int>(num_vars_checked);

    assert(var_order.size() == static_cast<size_t>(num_vars));

    std::vector<kitty::dynamic_truth_table> permuted_tts = ss_detail::permute_all_variables(tts, var_order);
    mockturtle::xag_network permuted = synthesize_selectswap_internal(permuted_tts, num_vars, k);
    return rewire_pis_with_order(permuted, var_order);
}

uint32_t count_ss_ands_with_order(std::vector<kitty::dynamic_truth_table> const &tts,
                                   std::vector<uint32_t> const &var_order, int k) {
    [[maybe_unused]] const uint32_t num_vars = tts.front().num_vars();
    assert(var_order.size() == num_vars);

    std::vector<kitty::dynamic_truth_table> permuted_tts = ss_detail::permute_all_variables(tts, var_order);
    auto [candidate, and_count] = try_split(permuted_tts, k);
    return and_count;
}

namespace {

void update_best_for_order(std::vector<kitty::dynamic_truth_table> const &tts,
                           std::vector<uint32_t> const &order, int k,
                           SSSensitivityResult &result) {
    const uint32_t num_vars = tts.front().num_vars();
    if (k == -1) {
        for (int current_k = 1; current_k < static_cast<int>(num_vars); ++current_k) {
            uint32_t and_count = count_ss_ands_with_order(tts, order, current_k);
            if (and_count < result.best_and_count) {
                result.best_and_count = and_count;
                result.best_k = current_k;
                result.best_order = order;
            }
        }
    } else {
        uint32_t and_count = count_ss_ands_with_order(tts, order, k);
        if (and_count < result.best_and_count) {
            result.best_and_count = and_count;
            result.best_k = k;
            result.best_order = order;
        }
    }
}

} // namespace

SSSensitivityResult find_best_variable_order(std::vector<kitty::dynamic_truth_table> const &tts,
                                              int k, VariableSelectionParams const &params) {
    const uint32_t num_vars = tts.front().num_vars();
    SSSensitivityResult result;
    result.best_and_count = std::numeric_limits<uint32_t>::max();

    if (params.method == VariableSelectionMethod::Random) {
        std::mt19937_64 rng(params.seed);
        std::vector<uint32_t> order(num_vars);
        std::iota(order.begin(), order.end(), 0);

        for (uint32_t trial = 0; trial < params.num_random_tries; ++trial) {
            std::shuffle(order.begin(), order.end(), rng);
            update_best_for_order(tts, order, k, result);
        }
    } else {
        std::vector<uint32_t> order = select_variables(tts, k == -1 ? 1 : k, params);
        update_best_for_order(tts, order, k, result);
    }

    return result;
}

} // namespace lut_synth
