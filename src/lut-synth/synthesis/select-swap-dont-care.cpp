#include "lut-synth/synthesis/synthesis.hpp"
#include "lut-synth/resynthesis/resynthesis-util.hpp"

#include <kitty/bit_operations.hpp>
#include <kitty/constructors.hpp>
#include <kitty/operations.hpp>
#include <kitty/properties.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/xag.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <utility>
#include <vector>

namespace lut_synth {

namespace {

struct ReshapedDC {
    std::vector<kitty::dynamic_truth_table> on;
    std::vector<kitty::dynamic_truth_table> off;
};

ReshapedDC reshape_dc(std::vector<kitty::dynamic_truth_table> const &on,
                      std::vector<kitty::dynamic_truth_table> const &off, int k, int num_vars) {
    int const m = static_cast<int>(on.size());
    int const remaining_vars = num_vars - k;
    int const num_cofactors = 1 << k;

    ReshapedDC reshaped;
    reshaped.on.resize(num_cofactors * m, kitty::dynamic_truth_table(remaining_vars));
    reshaped.off.resize(num_cofactors * m, kitty::dynamic_truth_table(remaining_vars));

    ss_detail::for_each_cofactor_entry(k, num_vars, m, [&](int cof_idx, int assignment, int output, int original_index) {
        if (kitty::get_bit(on[output], original_index)) {
            kitty::set_bit(reshaped.on[cof_idx * m + output], assignment);
        }
        if (kitty::get_bit(off[output], original_index)) {
            kitty::set_bit(reshaped.off[cof_idx * m + output], assignment);
        }
    });

    return reshaped;
}

kitty::dynamic_truth_table monomial_tt(int num_vars, int mask) {
    kitty::dynamic_truth_table term = ~kitty::dynamic_truth_table(static_cast<uint32_t>(num_vars));
    for (int var = 0; var < num_vars; ++var) {
        if ((mask >> var) & 1) {
            kitty::dynamic_truth_table v(static_cast<uint32_t>(num_vars));
            kitty::create_nth_var(v, var);
            term &= v;
        }
    }
    return term;
}

bool respects_dc(kitty::dynamic_truth_table const &f, kitty::dynamic_truth_table const &on,
                 kitty::dynamic_truth_table const &off) {
    if (!kitty::is_const0(f & off)) {
        return false;
    }
    if (!kitty::is_const0((~f) & on)) {
        return false;
    }
    return true;
}

kitty::dynamic_truth_table prune_anf_dc(kitty::dynamic_truth_table const &on,
                                        kitty::dynamic_truth_table const &off) {
    kitty::dynamic_truth_table f = on;
    kitty::dynamic_truth_table anf = ss_detail::to_anf(f);
    const int n = static_cast<int>(f.num_vars());
    const int limit = 1 << n;

    std::vector<int> monomials;
    monomials.reserve(limit);
    for (int i = 0; i < limit; ++i) {
        if (kitty::get_bit(anf, i)) {
            monomials.push_back(i);
        }
    }

    auto deg = [](int x) { return __builtin_popcount(static_cast<unsigned>(x)); };
    std::sort(monomials.begin(), monomials.end(),
              [&](int a, int b) { return deg(a) != deg(b) ? deg(a) > deg(b) : a > b; });

    for (int m : monomials) {
        kitty::dynamic_truth_table infl = monomial_tt(n, m);
        kitty::dynamic_truth_table cand = f ^ infl;
        if (respects_dc(cand, on, off)) {
            f = std::move(cand);
            kitty::clear_bit(anf, static_cast<uint64_t>(m));
        }
    }

    return anf;
}

bool verify_dc(mockturtle::xag_network const &network,
               std::vector<kitty::dynamic_truth_table> const &on,
               std::vector<kitty::dynamic_truth_table> const &off) {
    if (network.num_pis() != on.front().num_vars()) {
        return false;
    }
    const mockturtle::default_simulator<kitty::dynamic_truth_table> simulator(on.front().num_vars());
    const std::vector<kitty::dynamic_truth_table> simulation = mockturtle::simulate<kitty::dynamic_truth_table>(network, simulator);
    if (simulation.size() != on.size()) {
        return false;
    }
    for (size_t i = 0; i < on.size(); ++i) {
        if (!respects_dc(simulation[i], on[i], off[i])) {
            return false;
        }
    }
    return true;
}

mockturtle::xag_network synthesize_for_k_dc(std::vector<kitty::dynamic_truth_table> const &on,
                                            std::vector<kitty::dynamic_truth_table> const &off,
                                            int num_vars, int k) {
    assert(k >= 0 && k <= num_vars);

    const int num_outputs = static_cast<int>(on.size());

    mockturtle::xag_network network;
    std::vector<mockturtle::xag_network::signal> pis = ss_detail::create_pis(network, num_vars);

    std::vector<mockturtle::xag_network::signal> select_pis(pis.begin(), pis.begin() + k);
    std::vector<mockturtle::xag_network::signal> remaining_pis(pis.begin() + k, pis.end());

    const ReshapedDC reshaped = reshape_dc(on, off, k, num_vars);

    std::vector<kitty::dynamic_truth_table> anfs;
    anfs.reserve(reshaped.on.size());
    for (size_t i = 0; i < reshaped.on.size(); ++i) {
        anfs.push_back(prune_anf_dc(reshaped.on[i], reshaped.off[i]));
    }

    const std::vector<mockturtle::xag_network::signal> select_outputs = ss_detail::select_outputs_from_anfs(network, anfs, remaining_pis);

    ss_detail::build_mux_tree(network, select_outputs, select_pis, num_outputs, k);

    return mockturtle::cleanup_dangling(network);
}

std::pair<mockturtle::xag_network, uint32_t>
try_split_dc(std::vector<kitty::dynamic_truth_table> const &on,
             std::vector<kitty::dynamic_truth_table> const &off, int k) {
    mockturtle::xag_network network = synthesize_for_k_dc(on, off, static_cast<int>(on.front().num_vars()), k);
    if (!verify_dc(network, on, off)) {
        return {mockturtle::xag_network{}, std::numeric_limits<uint32_t>::max()};
    }
    uint32_t and_count = count_ands(network);
    return {std::move(network), and_count};
}

uint32_t check_dc(std::vector<kitty::dynamic_truth_table> const &on,
                  std::vector<kitty::dynamic_truth_table> const &off) {
    const uint32_t num_vars = check_tts(on, "synthesize_selectswap_dontcare");
    (void)check_tts(off, "synthesize_selectswap_dontcare");
    assert(on.size() == off.size());
    for (size_t i = 0; i < on.size(); ++i) {
        assert(on[i].num_vars() == off[i].num_vars());
        assert(kitty::is_const0(on[i] & off[i]));
    }
    return num_vars;
}

} // namespace

mockturtle::xag_network synthesize_selectswap_dontcare_internal(std::vector<kitty::dynamic_truth_table> const &on,
                                                   std::vector<kitty::dynamic_truth_table> const &off,
                                                   int num_vars, int k) {
    int best_k = k;
    if (k == -1) {
        mockturtle::xag_network best_network;
        uint32_t best_and = std::numeric_limits<uint32_t>::max();
        uint32_t best_size = std::numeric_limits<uint32_t>::max();
        for (int current_k = 1; current_k < num_vars; ++current_k) {
            auto [candidate, and_count] = try_split_dc(on, off, current_k);
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
    auto [network, and_count] = try_split_dc(on, off, best_k);
    assert(and_count != std::numeric_limits<uint32_t>::max());
    return network;
}

mockturtle::xag_network synthesize_selectswap_dontcare(std::vector<kitty::dynamic_truth_table> const &on,
                                        std::vector<kitty::dynamic_truth_table> const &off, int k,
                                        uint32_t num_random_starts, uint64_t seed) {
    const int num_vars = static_cast<int>(check_dc(on, off));

    if (num_random_starts <= 1) {
        return synthesize_selectswap_dontcare_internal(on, off, num_vars, k);
    }

    mockturtle::xag_network best_network;
    uint32_t best_and = std::numeric_limits<uint32_t>::max();

    std::mt19937_64 rng(seed);
    std::vector<uint32_t> order(num_vars);
    std::iota(order.begin(), order.end(), 0);

    for (uint32_t trial = 0; trial < num_random_starts; ++trial) {
        std::shuffle(order.begin(), order.end(), rng);
        std::vector<kitty::dynamic_truth_table> permuted_on = ss_detail::permute_all_variables(on, order);
        std::vector<kitty::dynamic_truth_table> permuted_off = ss_detail::permute_all_variables(off, order);
        mockturtle::xag_network candidate = synthesize_selectswap_dontcare_internal(permuted_on, permuted_off, num_vars, k);
        uint32_t and_count = count_ands(candidate);

        if (and_count < best_and) {
            best_and = and_count;
            best_network = std::move(candidate);
        }
    }

    return best_network;
}

} // namespace lut_synth
