#include "lut-synth/resynthesis/resynthesis.hpp"
#include "lut-synth/resynthesis/resynthesis-util.hpp"

#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>

namespace lut_synth {

namespace {

xag_network optimize_round(xag_network xag,
                               AnySynParams const &params,
                               uint32_t cut_size,
                               xag_network const &original,
                               Deadline const &dl) {
    mockturtle::xag_npn_resynthesis<xag_network> npn_resyn;
    mockturtle::cut_rewriting_params crps;
    crps.cut_enumeration_ps.cut_size = 4;

    uint32_t best_and = count_ands(xag);
    xag_network best = xag;

    for (uint32_t iter = 0; iter < params.max_iterations_per_round; ++iter) {
        if (dl.expired()) break;

        uint32_t start_and = count_ands(xag);

        CostGenericResubStats resub_stats;
        xag = apply_cost_generic_resub(xag, params.resub_params, &resub_stats);

        xag = apply_cut_rewriting(xag, npn_resyn, crps);

        if (params.use_minmc_database) {
            xag = apply_cut_rewriting_minmc(xag, cut_size, params.use_dont_cares,
                                                params.cut_limit, false);

            if (params.allow_zero_gain && iter == 0) {
                xag = apply_cut_rewriting_minmc(xag, cut_size, params.use_dont_cares,
                                                    params.cut_limit, true);
            }
        }

        if (params.use_minmc_dc_resub) {
            xag = apply_resub_minmc_withdc(xag, params.minmc_dc_resub_params);
        }

        if (params.use_dc_and_rewrite) {
            xag = apply_dc_and_rewrite(xag, params.dc_and_rewrite_params);
        }

        uint32_t cur_and = count_ands(xag);
        if (cur_and < best_and && check_equiv(original, xag)) {
            best = xag;
            best_and = cur_and;
        }

        if (cur_and >= start_and) {
            break;
        }
    }

    return best;
}

} // anonymous namespace

ResynthesisResult resynthesize_xag_anysyn_with_report(xag_network const &ntk,
                                                   AnySynParams const &params) {
    Deadline total(params.timeout_s);

    ResynthesisResult result;
    ResynthesisReportData &data = result.report_data;

    data.num_inputs = ntk.num_pis();
    data.num_outputs = ntk.num_pos();
    data.initial_and_count = count_ands(ntk);
    data.initial_xor_count = count_xors(ntk);
    data.max_iterations = params.optimization_rounds;

    ResynthesisV2State state{ntk, data.initial_and_count, &ntk};

    std::vector<xag_network> candidates;
    candidates.push_back(ntk);

    if (params.use_esop_once && !total.expired()) {
        xag_network esop_xag = apply_esop_balance(ntk);
        uint32_t esop_and = count_ands(esop_xag);
        if (esop_and <= static_cast<uint32_t>(data.initial_and_count * 1.1) &&
            check_equiv(ntk, esop_xag)) {
            candidates.push_back(std::move(esop_xag));
        }
    }

    if (params.use_klut_once && !total.expired()) {
        xag_network klut_xag = mockturtle::cleanup_dangling(
            apply_klut_decompress(ntk, params.klut_size));
        uint32_t klut_and = count_ands(klut_xag);
        if (klut_and <= static_cast<uint32_t>(data.initial_and_count * 1.1) &&
            check_equiv(ntk, klut_xag)) {
            candidates.push_back(std::move(klut_xag));
        }
    }

    for (auto const &candidate : candidates) {
        if (total.expired()) break;

        for (uint32_t round = 0; round < params.optimization_rounds; ++round) {
            if (total.expired()) break;

            resynth_clock::time_point round_start = resynth_clock::now();
            ResynthesisIterationInfo iter_info;
            iter_info.iteration = round + 1;
            iter_info.and_count_start = state.global_best_and;

            uint32_t cut_size_idx = round % params.minmc_cut_sizes.size();
            uint32_t cut_size = params.minmc_cut_sizes[cut_size_idx];

            xag_network optimized = optimize_round(candidate, params, cut_size, ntk, total);
            uint32_t opt_and = count_ands(optimized);

            state.update_if_better(std::move(optimized), opt_and);

            iter_info.and_count_end = state.global_best_and;
            iter_info.iteration_time_ms = elapsed_ms(round_start, resynth_clock::now());
            data.iterations.push_back(iter_info);
            data.actual_iterations = round + 1;

            if (iter_info.and_count_end >= iter_info.and_count_start) {
                break;
            }
        }
    }

    data.total_time_ms = total.elapsed_ms();
    data.final_and_count = state.global_best_and;
    data.final_xor_count = count_xors(state.global_best);

    result.xag = state.global_best;
    return result;
}

xag_network resynthesize_xag_anysyn(xag_network const &ntk,
                                 AnySynParams const &params) {
    return resynthesize_xag_anysyn_with_report(ntk, params).xag;
}

} // namespace lut_synth
