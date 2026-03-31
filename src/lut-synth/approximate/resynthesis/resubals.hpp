#pragma once

#include <cstdint>
#include <vector>

#include <mockturtle/networks/xag.hpp>

#include "lut-synth/approximate/resynthesis/error-estimator.hpp"

namespace lut_synth::approximate {

/*! \brief Estimator type for ResubALS. */
enum class EstimatorType {
    Simulation,  /*!< Direct simulation-based estimation */
    VECBEE,      /*!< VECBEE estimation */
    Miter,       /*!< Miter-based Boolean difference estimation */
    Integer      /*!< Integer-aware estimation (weighted absolute integer error) */
};

/*! \brief Parameters for ResubALS algorithm. */
struct ResubALSParams {
    ErrorMetric metric = ErrorMetric::ER;      /*!< Error metric type */
    double error_bound = 0.05;                 /*!< Maximum error bound */
    uint32_t num_patterns = 102400;            /*!< Number of simulation patterns */
    uint32_t max_divisors = 150;               /*!< Maximum divisors */
    uint32_t max_lac_size = 2;                 /*!< Max LAC size (0-2 resub) */
    EstimatorType estimator = EstimatorType::Simulation;  /*!< Estimator type */
    bool use_vecbee = false;                   /*!< Use VECBEE (deprecated, use estimator) */
    bool use_knapsack = true;                  /*!< Use knapsack in phase 1 */
    uint32_t seed = 0;                         /*!< Random seed */
    std::vector<double> weights;               /*!< Per-input weights for Integer estimator */
};

/*! \brief Statistics from ResubALS execution. */
struct ResubALSStats {
    uint32_t original_size = 0;    /*!< Original network size */
    uint32_t final_size = 0;       /*!< Final network size */
    uint32_t lacs_applied = 0;     /*!< Number of LACs applied */
    double actual_error = 0.0;     /*!< Actual error rate (for compatibility) */
    double phase1_gain = 0.0;      /*!< Size gain from phase 1 */
    double phase2_gain = 0.0;      /*!< Size gain from phase 2 */

    uint64_t error_count = 0;       /*!< Error count in patterns (integer) */
    uint64_t num_patterns = 0;      /*!< Total simulation patterns */
    uint32_t rollbacks = 0;         /*!< Number of rollbacks performed */
    bool verified = true;           /*!< Error verification passed */
};

/*! \brief Result from ResubALS execution. */
struct ResubALSResult {
    mockturtle::xag_network network;  /*!< Approximated XAG network */
    ResubALSStats stats;               /*!< Execution statistics */
};

/*! \brief ResubALS 3-phase approximate synthesis algorithm.
 *
 *  Phase 1: Multiple selection using knapsack optimization
 *  Phase 2: Iterative single selection until budget exhausted
 *  Phase 3: Cleanup dangling nodes
 *
 *  \param ntk Input XAG network
 *  \param params Algorithm parameters
 *  \return Result containing approximated network and statistics
 */
ResubALSResult resubals(mockturtle::xag_network const& ntk,
                        ResubALSParams const& params = {});

/*! \brief ResubALS with out-parameter (deprecated).
 *
 *  \deprecated Use resubals() returning ResubALSResult instead.
 *  \param ntk Input XAG network
 *  \param params Algorithm parameters
 *  \param stats Output statistics
 *  \return Approximated XAG network
 */
[[deprecated("Use resubals() returning ResubALSResult instead")]]
mockturtle::xag_network resubals(mockturtle::xag_network const& ntk,
                                  ResubALSParams const& params,
                                  ResubALSStats* stats);

} // namespace lut_synth::approximate
