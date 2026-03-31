#pragma once

#include <cstdint>

#include <mockturtle/networks/xag.hpp>

#include "lut-synth/error.hpp"
#include "lut-synth/resynthesis/resynthesis-stats.hpp"
#include "lut-synth/approximate/resynthesis/approximate.hpp"
#include "lut-synth/resynthesis/cost-generic-resub.hpp"
#include "lut-synth/resynthesis/xag-resynthesizer.hpp"

namespace lut_synth {

/*! \brief Result of XAG resynthesis with report data. */
struct ResynthesisResult {
    mockturtle::xag_network xag;           /*!< Resynthesized network */
    ResynthesisReportData report_data; /*!< Report data */
};

/*! \brief Parameters for v3 resynthesis focusing on downhill optimization. */
struct ResynthesisV3Params {
    double timeout_s{60.0};                           /*!< Total time budget */
    CostGenericResubParams resub_params{};            /*!< Base resubstitution params */
    uint32_t fast_max_divisors{50};                   /*!< Divisors for fast resub */
    uint32_t full_max_divisors{150};                  /*!< Divisors for polish resub */
    std::vector<uint32_t> klut_sizes{4, 5, 6};       /*!< KLUT sizes for perturbation */

    bool use_minmc_database{true};                    /*!< Use minmc database lookup */
    std::vector<uint32_t> minmc_cut_sizes{4, 5};     /*!< Cut sizes to try (max 5) */
    bool use_dont_cares{true};                        /*!< Enable DC in cut_rewriting */
    uint32_t cut_limit{25};                           /*!< Cuts per node for enumeration */
    bool allow_zero_gain{false};                      /*!< Accept equal cost moves */

    uint32_t optimization_rounds{3};                  /*!< Rounds with different params */
    uint32_t max_iterations_per_round{5};             /*!< Iterations per round */

    bool use_esop_once{true};                         /*!< Try ESOP once at start */
    bool use_klut_once{false};                        /*!< Try KLUT once at start */
    uint32_t klut_size{5};                            /*!< KLUT size if enabled */
};

/*! \brief Resynthesize XAG using v3 algorithm with multi-pass optimization.
 *  \param xag Input XAG network
 *  \param params V3 resynthesis parameters
 *  \return Optimized XAG network
 */
mockturtle::xag_network resynthesize_xag_v3(mockturtle::xag_network const &xag,
                                             ResynthesisV3Params const &params = {});

/*! \brief Resynthesize XAG using v3 algorithm and return report data.
 *  \param xag Input XAG network
 *  \param params V3 resynthesis parameters
 *  \return Result with optimized network and report
 */
ResynthesisResult resynthesize_xag_v3_with_report(mockturtle::xag_network const &xag,
                                                   ResynthesisV3Params const &params = {});

/*! \brief Rewrite XAG using multiplicative complexity cost. */
mockturtle::xag_network rewrite_with_mc_cost(mockturtle::xag_network const &ntk);

/*! \brief Parameters for approximate resubstitution (legacy alias). */
struct approximate_resub_params {
    ErrorMetric metric = ErrorMetric::ER; /*!< Error metric type */
    double error_bound = 0.05;            /*!< Maximum error bound */
    uint32_t num_patterns = 102400;       /*!< Simulation patterns */
    uint32_t max_divisors = 150;          /*!< Maximum divisors */
    uint32_t seed = 0;                    /*!< Random seed */
};

/*! \brief Approximate resubstitution within error bound (legacy wrapper). */
inline mockturtle::xag_network approximate_resubstitution(mockturtle::xag_network const &ntk,
                                                          approximate_resub_params const &ps = {}) {
    approximate::ResubALSParams params;
    params.metric = ps.metric;
    params.error_bound = ps.error_bound;
    params.num_patterns = ps.num_patterns;
    params.max_divisors = ps.max_divisors;
    params.seed = ps.seed;
    return approximate::resubals(ntk, params).network;
}

/*! \brief Parameters for exact rewriting. */
struct ExactRewriteParams {
    uint32_t cut_size = 4;        /*!< Maximum cut size */
    uint32_t max_iterations = 3;  /*!< Maximum iterations */
    bool verbose = false;         /*!< Enable verbose output */
};

/*! \brief Rewrite XAG using exact multiplicative complexity synthesis.
 *  \param ntk Input XAG network
 *  \param params Rewrite parameters
 *  \return Optimized XAG network
 */
mockturtle::xag_network rewrite_exact_mc(mockturtle::xag_network const &ntk,
                                         ExactRewriteParams const &params = {});

} // namespace lut_synth
