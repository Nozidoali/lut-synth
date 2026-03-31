#pragma once

#include <cstdint>
#include <mockturtle/networks/xag.hpp>

namespace lut_synth {

/*! \brief Parameters for cost-generic resubstitution. */
struct CostGenericResubParams {
    uint32_t max_pis = 8;                         /*!< Maximum primary inputs in window */
    uint32_t max_divisors = 150;                  /*!< Maximum divisors to consider */
    uint32_t skip_fanout_limit_for_roots = 1000;  /*!< Skip roots with high fanout */
    uint32_t skip_fanout_limit_for_divisors = 100; /*!< Skip divisors with high fanout */
};

/*! \brief Statistics from cost-generic resubstitution. */
struct CostGenericResubStats {
    double time_total_ms = 0.0;      /*!< Total runtime in milliseconds */
    uint32_t initial_size = 0;       /*!< Initial network size */
    uint32_t num_windows = 0;        /*!< Number of windows processed */
    uint32_t num_substitutions = 0;  /*!< Number of substitutions made */
};

/*! \brief Result from cost-generic resubstitution. */
struct CostGenericResubResult {
    mockturtle::xag_network network;  /*!< Optimized XAG network */
    CostGenericResubStats stats;       /*!< Execution statistics */
};

/*! \brief Cost-generic resubstitution for multiplicative complexity.
 *  \param ntk Input XAG network
 *  \param ps Resubstitution parameters
 *  \return Result containing optimized network and statistics
 */
CostGenericResubResult cost_generic_resub_mc(mockturtle::xag_network const& ntk,
                                              CostGenericResubParams const& ps = {});

/*! \brief Cost-generic resubstitution with out-parameter (deprecated).
 *
 *  \deprecated Use cost_generic_resub_mc() returning CostGenericResubResult instead.
 *  \param ntk Input XAG network
 *  \param ps Resubstitution parameters
 *  \param pst Output statistics
 *  \return Optimized XAG network
 */
[[deprecated("Use cost_generic_resub_mc() returning CostGenericResubResult instead")]]
mockturtle::xag_network cost_generic_resub_mc(mockturtle::xag_network const& ntk,
                                              CostGenericResubParams const& ps,
                                              CostGenericResubStats* pst);

}  // namespace lut_synth
