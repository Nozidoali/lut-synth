#include "lut-synth/synthesis/synthesis.hpp"
#include "lut-synth/resynthesis/resynthesis.hpp"
#include "lut-synth/resynthesis/resynthesis-util.hpp"

#include <algorithm>
#include <cassert>

#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/exact_mc_synthesis.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <kitty/print.hpp>

namespace lut_synth {

namespace {

mockturtle::xag_network combine_single_outputs(
    std::vector<mockturtle::xag_network> const &single_output_xags, uint32_t num_vars) {

    if (single_output_xags.empty()) {
        return mockturtle::xag_network{};
    }

    if (single_output_xags.size() == 1) {
        return single_output_xags[0];
    }

    mockturtle::xag_network combined;
    std::vector<mockturtle::xag_network::signal> pis(num_vars);
    std::generate(pis.begin(), pis.end(), [&]() { return combined.create_pi(); });

    for (auto const &xag : single_output_xags) {
        std::unordered_map<mockturtle::xag_network::node, mockturtle::xag_network::signal> node_map;
        node_map[xag.get_node(xag.get_constant(false))] = combined.get_constant(false);

        uint32_t pi_idx = 0;
        xag.foreach_pi([&](auto n) {
            node_map[n] = pis[pi_idx++];
        });

        xag.foreach_gate([&](auto n) {
            std::vector<mockturtle::xag_network::signal> children;
            xag.foreach_fanin(n, [&](auto f) {
                std::unordered_map<mockturtle::xag_network::node, mockturtle::xag_network::signal>::iterator it = node_map.find(xag.get_node(f));
                assert(it != node_map.end());
                children.push_back(xag.is_complemented(f) ? combined.create_not(it->second) : it->second);
            });

            if (xag.is_and(n)) {
                node_map[n] = combined.create_and(children[0], children[1]);
            } else {
                node_map[n] = combined.create_xor(children[0], children[1]);
            }
        });

        xag.foreach_po([&](auto f) {
            std::unordered_map<mockturtle::xag_network::node, mockturtle::xag_network::signal>::iterator it = node_map.find(xag.get_node(f));
            assert(it != node_map.end());
            mockturtle::xag_network::signal sig = xag.is_complemented(f) ? combined.create_not(it->second) : it->second;
            combined.create_po(sig);
        });
    }

    return mockturtle::cleanup_dangling(combined);
}

} // namespace

mockturtle::xag_network synthesize_exact(kitty::dynamic_truth_table const &tt,
                                          ExactSynthesisParams const &params,
                                          ExactSynthesisStats *stats) {
    if (tt.num_vars() > params.max_vars) {
        return synthesize_selectswap({tt}, -1);
    }

    mockturtle::exact_mc_synthesis_params mc_params;
    mc_params.use_cegar = params.use_cegar;
    mc_params.verbose = params.verbose;

    mockturtle::exact_mc_synthesis_stats mc_stats;
    mockturtle::xag_network result = mockturtle::exact_mc_synthesis(tt, mc_params, &mc_stats);

    if (stats) {
        stats->total_time_ms =
            static_cast<uint64_t>(mockturtle::to_seconds(mc_stats.time_total) * 1000);
        stats->min_and_found = count_ands(result);
    }

    return result;
}

mockturtle::xag_network synthesize_exact(std::vector<kitty::dynamic_truth_table> const &tts,
                                          ExactSynthesisParams const &params) {
    const uint32_t num_vars = check_tts(tts, "synthesize_exact");

    if (num_vars > params.max_vars) {
        return synthesize_selectswap(tts, -1);
    }

    std::vector<mockturtle::xag_network> single_outputs;
    single_outputs.reserve(tts.size());

    for (auto const &tt : tts) {
        single_outputs.push_back(synthesize_exact(tt, params, nullptr));
    }

    return combine_single_outputs(single_outputs, num_vars);
}

mockturtle::xag_network rewrite_exact_mc(mockturtle::xag_network const &ntk,
                                          ExactRewriteParams const &params) {
    mockturtle::xag_network result = ntk;

    mockturtle::cut_rewriting_params cut_params;
    cut_params.cut_enumeration_ps.cut_size = params.cut_size;
    cut_params.cut_enumeration_ps.cut_limit = 16;
    cut_params.progress = false;

    mockturtle::xag_npn_resynthesis<mockturtle::xag_network> resyn;

    uint32_t best_and = count_ands(result);

    for (uint32_t iter = 0; iter < params.max_iterations; ++iter) {
        mockturtle::xag_network candidate = mockturtle::cleanup_dangling(mockturtle::cut_rewriting(result, resyn, cut_params));
        uint32_t cand_and = count_ands(candidate);

        if (cand_and < best_and) {
            result = std::move(candidate);
            best_and = cand_and;
        } else {
            break;
        }
    }

    return result;
}

CECResult check_equivalence(mockturtle::xag_network const &ntk1,
                            mockturtle::xag_network const &ntk2) {
    CECResult result;

    if (ntk1.num_pis() != ntk2.num_pis()) {
        result.error_message = "Networks have different number of primary inputs";
        return result;
    }

    if (ntk1.num_pos() != ntk2.num_pos()) {
        result.error_message = "Networks have different number of primary outputs";
        return result;
    }

    const uint32_t num_vars = ntk1.num_pis();
    if (num_vars > 16) {
        result.error_message = "Too many variables for exhaustive simulation";
        return result;
    }

    mockturtle::default_simulator<kitty::dynamic_truth_table> sim1(num_vars);
    mockturtle::default_simulator<kitty::dynamic_truth_table> sim2(num_vars);

    std::vector<kitty::dynamic_truth_table> tts1 = mockturtle::simulate<kitty::dynamic_truth_table>(ntk1, sim1);
    std::vector<kitty::dynamic_truth_table> tts2 = mockturtle::simulate<kitty::dynamic_truth_table>(ntk2, sim2);

    result.completed = true;

    for (size_t i = 0; i < tts1.size(); ++i) {
        if (tts1[i].num_vars() != tts2[i].num_vars() ||
            kitty::to_hex(tts1[i]) != kitty::to_hex(tts2[i])) {
            result.equivalent = false;

            if (tts1[i].num_vars() == tts2[i].num_vars()) {
                for (uint64_t pattern = 0; pattern < (1ULL << num_vars); ++pattern) {
                    bool val1 = kitty::get_bit(tts1[i], pattern);
                    bool val2 = kitty::get_bit(tts2[i], pattern);
                    if (val1 != val2) {
                        result.counter_example.resize(num_vars);
                        for (uint32_t j = 0; j < num_vars; ++j) {
                            result.counter_example[j] = (pattern >> j) & 1;
                        }
                        break;
                    }
                }
            }
            return result;
        }
    }

    result.equivalent = true;
    return result;
}

CECResult check_equivalence(mockturtle::xag_network const &ntk,
                            std::vector<kitty::dynamic_truth_table> const &tts) {
    CECResult result;

    if (ntk.num_pis() == 0 && tts.empty()) {
        result.completed = true;
        result.equivalent = true;
        return result;
    }

    if (ntk.num_pos() != tts.size()) {
        result.error_message = "Network outputs don't match number of truth tables";
        return result;
    }

    const uint32_t num_vars = tts.empty() ? ntk.num_pis() : tts.front().num_vars();
    if (ntk.num_pis() != num_vars) {
        result.error_message = "Network inputs don't match truth table variables";
        return result;
    }

    if (num_vars > 16) {
        result.error_message = "Too many variables for exhaustive simulation";
        return result;
    }

    mockturtle::default_simulator<kitty::dynamic_truth_table> sim(num_vars);
    std::vector<kitty::dynamic_truth_table> ntk_tts = mockturtle::simulate<kitty::dynamic_truth_table>(ntk, sim);

    result.completed = true;

    for (size_t i = 0; i < tts.size(); ++i) {
        if (ntk_tts[i] != tts[i]) {
            result.equivalent = false;

            for (uint64_t pattern = 0; pattern < (1ULL << num_vars); ++pattern) {
                bool val_ntk = kitty::get_bit(ntk_tts[i], pattern);
                bool val_tt = kitty::get_bit(tts[i], pattern);
                if (val_ntk != val_tt) {
                    result.counter_example.resize(num_vars);
                    for (uint32_t j = 0; j < num_vars; ++j) {
                        result.counter_example[j] = (pattern >> j) & 1;
                    }
                    break;
                }
            }
            return result;
        }
    }

    result.equivalent = true;
    return result;
}

} // namespace lut_synth
