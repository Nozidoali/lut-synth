#include "lut-synth/synthesis/rmdds.hpp"
#include "lut-synth/synthesis/anf-xag-builder.hpp"
#include "lut-synth/synthesis/synthesis.hpp"
#include "lut-synth/resynthesis/resynthesis-util.hpp"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <unordered_map>

#include <kitty/operations.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/simulation.hpp>

namespace lut_synth {

namespace {

using signal = xag_network::signal;

uint32_t compute_influence(kitty::dynamic_truth_table const &tt, uint32_t var) {
    kitty::dynamic_truth_table c0 = kitty::cofactor0(tt, var);
    kitty::dynamic_truth_table c1 = kitty::cofactor1(tt, var);
    uint32_t diff_count = 0;
    uint64_t num_bits = 1ull << (tt.num_vars() - 1);
    for (uint64_t i = 0; i < num_bits; ++i) {
        if (kitty::get_bit(c0, i) != kitty::get_bit(c1, i)) {
            ++diff_count;
        }
    }
    return diff_count;
}

std::vector<uint32_t> compute_variable_ordering_multi(
    std::vector<kitty::dynamic_truth_table> const &tts) {
    uint32_t num_vars = tts.front().num_vars();
    std::vector<std::pair<uint32_t, uint32_t>> influences;
    influences.reserve(num_vars);
    for (uint32_t v = 0; v < num_vars; ++v) {
        uint32_t total = 0;
        for (auto const &tt : tts) {
            total += compute_influence(tt, v);
        }
        influences.emplace_back(total, v);
    }
    std::sort(influences.begin(), influences.end(), std::greater<>());
    std::vector<uint32_t> ordering;
    ordering.reserve(num_vars);
    for (auto const &p : influences) {
        ordering.push_back(p.second);
    }
    return ordering;
}

xag_network synthesize_rmdds_with_ordering(std::vector<kitty::dynamic_truth_table> const &tts,
                                           std::vector<uint32_t> const &var_ordering,
                                           bool enable_term_reordering) {
    uint32_t num_vars = tts.front().num_vars();
    xag_network network;

    if (num_vars == 0) {
        for (auto const &tt : tts) {
            network.create_po(kitty::get_bit(tt, 0) ? network.get_constant(true)
                                                    : network.get_constant(false));
        }
        return network;
    }

    std::vector<signal> pis;
    pis.reserve(num_vars);
    for (uint32_t i = 0; i < num_vars; ++i) {
        pis.push_back(network.create_pi());
    }

    std::vector<signal> reordered_inputs(num_vars);
    for (uint32_t i = 0; i < num_vars; ++i) {
        reordered_inputs[i] = pis[var_ordering[i]];
    }

    std::vector<uint32_t> all_terms;
    std::vector<std::vector<uint32_t>> output_terms;
    output_terms.reserve(tts.size());

    for (auto const &tt : tts) {
        kitty::dynamic_truth_table permuted = tt.construct();
        for (uint64_t i = 0; i < (1ull << num_vars); ++i) {
            uint64_t new_idx = 0;
            for (uint32_t v = 0; v < num_vars; ++v) {
                if (i & (1ull << var_ordering[v])) {
                    new_idx |= (1ull << v);
                }
            }
            if (kitty::get_bit(tt, i)) {
                kitty::set_bit(permuted, new_idx);
            }
        }

        kitty::dynamic_truth_table anf = ss_detail::to_anf(permuted);
        std::vector<uint32_t> terms = extract_anf_terms(anf);

        if (enable_term_reordering) {
            terms = reorder_terms_by_hamming(terms);
        }

        for (uint32_t t : terms) {
            if (std::find(all_terms.begin(), all_terms.end(), t) == all_terms.end()) {
                all_terms.push_back(t);
            }
        }
        output_terms.push_back(std::move(terms));
    }

    std::unordered_map<uint32_t, signal> cache;

    for (auto const &terms : output_terms) {
        signal out_signal = build_xag_from_terms(network, reordered_inputs, terms, cache);
        network.create_po(out_signal);
    }

    return mockturtle::cleanup_dangling(network);
}

std::vector<std::vector<uint32_t>> generate_orderings(uint32_t num_vars, uint32_t max_orderings,
                                                       std::vector<uint32_t> const &base_ordering) {
    std::vector<std::vector<uint32_t>> orderings;
    orderings.push_back(base_ordering);

    if (orderings.size() >= max_orderings) {
        return orderings;
    }

    std::vector<uint32_t> identity(num_vars);
    std::iota(identity.begin(), identity.end(), 0u);
    if (identity != base_ordering) {
        orderings.push_back(identity);
        if (orderings.size() >= max_orderings) {
            return orderings;
        }
    }

    std::vector<uint32_t> reverse = base_ordering;
    std::reverse(reverse.begin(), reverse.end());
    if (reverse != base_ordering && reverse != identity) {
        orderings.push_back(reverse);
        if (orderings.size() >= max_orderings) {
            return orderings;
        }
    }

    uint64_t max_possible = 1;
    for (uint32_t i = 2; i <= num_vars; ++i) {
        max_possible *= i;
    }

    std::mt19937 rng(42);
    uint32_t attempts = 0;
    uint32_t max_attempts = std::min(static_cast<uint32_t>(max_possible * 2), 1000u);
    while (orderings.size() < max_orderings && attempts < max_attempts) {
        ++attempts;
        std::vector<uint32_t> perm = base_ordering;
        std::shuffle(perm.begin(), perm.end(), rng);
        bool duplicate = false;
        for (auto const &existing : orderings) {
            if (perm == existing) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            orderings.push_back(perm);
        }
    }

    return orderings;
}

} // namespace

xag_network synthesize_rmdds(std::vector<kitty::dynamic_truth_table> const &tts,
                             RMDDSParams const &params,
                             RMDDSStats *stats) {
    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    check_tts(tts, "synthesize_rmdds");
    uint32_t num_vars = tts.front().num_vars();

    if (num_vars == 0) {
        xag_network network;
        for (auto const &tt : tts) {
            network.create_po(kitty::get_bit(tt, 0) ? network.get_constant(true)
                                                    : network.get_constant(false));
        }
        if (stats) {
            stats->num_terms = 0;
            stats->and_count = 0;
            stats->time_ms = 0.0;
        }
        return network;
    }

    std::vector<uint32_t> base_ordering = compute_variable_ordering_multi(tts);
    std::vector<std::vector<uint32_t>> orderings = generate_orderings(num_vars, params.max_var_orderings, base_ordering);

    xag_network best_network;
    uint32_t best_and_count = UINT32_MAX;

    for (auto const &ordering : orderings) {
        xag_network network = synthesize_rmdds_with_ordering(tts, ordering, params.enable_term_reordering);
        uint32_t and_count = count_ands(network);
        if (and_count < best_and_count) {
            best_and_count = and_count;
            best_network = std::move(network);
        }
    }

    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

    if (stats) {
        stats->and_count = best_and_count;
        stats->time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        uint32_t total_terms = 0;
        for (auto const &tt : tts) {
            kitty::dynamic_truth_table anf = ss_detail::to_anf(tt);
            total_terms += kitty::count_ones(anf);
        }
        stats->num_terms = total_terms;
    }

    return best_network;
}

xag_network apply_rmdds_optimize(xag_network const &xag, RMDDSParams const &params) {
    uint32_t num_inputs = xag.num_pis();
    uint32_t num_outputs = xag.num_pos();

    if (num_inputs > 16) {
        return xag;
    }

    std::vector<kitty::dynamic_truth_table> tts;
    tts.reserve(num_outputs);

    mockturtle::default_simulator<kitty::dynamic_truth_table> sim(num_inputs);
    std::vector<kitty::dynamic_truth_table> results = mockturtle::simulate<kitty::dynamic_truth_table>(xag, sim);

    for (uint32_t i = 0; i < num_outputs; ++i) {
        tts.push_back(results[i]);
    }

    return synthesize_rmdds(tts, params);
}

} // namespace lut_synth
