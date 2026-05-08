#pragma once

#include <chrono>
#include <cstdint>

#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/networks/xag.hpp>

#include "lut-synth/resynthesis/deadline.hpp"
#include "lut-synth/resynthesis/resynthesis-stats.hpp"
#include "lut-synth/resynthesis/cost-generic-resub.hpp"

namespace lut_synth {

using xag_network = mockturtle::xag_network;
using resynth_clock = std::chrono::high_resolution_clock;

uint32_t count_ands(xag_network const &xag);

uint32_t count_xors(xag_network const &xag);

bool abc_cec(xag_network const &ref, xag_network const &cand);

bool check_equiv(xag_network const &ref, xag_network const &cand);

xag_network apply_klut_decompress(xag_network const &xag, uint32_t lut_size);

xag_network apply_cost_generic_resub(xag_network const &xag,
                                      CostGenericResubParams const &ps,
                                      CostGenericResubStats *stats = nullptr);

xag_network apply_cut_rewriting(xag_network const &xag,
                                 mockturtle::xag_npn_resynthesis<xag_network> &resyn,
                                 mockturtle::cut_rewriting_params const &ps);

xag_network apply_esop_balance(xag_network const &xag);

xag_network apply_cut_rewriting_minmc(xag_network const &xag, uint32_t cut_size);

double elapsed_ms(resynth_clock::time_point start, resynth_clock::time_point end);

xag_network apply_algebraic_rewriting(xag_network const &xag);

ResynthesisStepInfo make_step(uint32_t num, std::string const &type,
                                       uint32_t before, uint32_t after, double ms);

xag_network apply_cut_rewriting_minmc_v3(xag_network const &xag,
                                          uint32_t cut_size,
                                          bool use_dont_cares,
                                          uint32_t cut_limit,
                                          bool allow_zero_gain);

xag_network apply_constant_fanin_opt(xag_network const &xag);

xag_network apply_dont_cares_opt(xag_network const &xag);

/*! \brief Parameters for SDC-aware multiplicative-complexity resubstitution.
 *
 *  Wraps mockturtle::resubstitution_params with knobs that affect the
 *  per-window SDC computation in resubstitution_minmc_withDC. Note that
 *  observability don't-cares are NOT used by this engine; only window_size
 *  and max_pis tune the effective SDC region.
 */
struct ResubMinMcDcParams {
    uint32_t window_size{150};   /*!< SDC TFI window size (mockturtle default 12) */
    uint32_t max_pis{8};         /*!< Max window leaves (mockturtle default 8) */
    uint32_t max_inserts{2};     /*!< Max inserts per resub (mockturtle default 2) */
    uint32_t max_divisors{150};  /*!< Max divisors per window (mockturtle default 150) */
};

xag_network apply_resub_minmc_withdc(xag_network const &xag,
                                      ResubMinMcDcParams const &ps = {});

xag_network apply_functional_reduction_pass(xag_network const &xag);

xag_network apply_refactoring_minmc(xag_network const &xag, bool use_dont_cares);

struct ResynthesisV2State {
    xag_network global_best;
    uint32_t global_best_and;
    xag_network const *original;

    void update_if_better(xag_network cand, uint32_t cand_and);
};

} // namespace lut_synth
