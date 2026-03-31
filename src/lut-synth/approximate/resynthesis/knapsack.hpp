#pragma once

#include <cstdint>
#include <vector>

#include "lut-synth/approximate/resynthesis/lac.hpp"

namespace lut_synth::approximate {

/*! \brief Result of knapsack LAC selection. */
struct KnapsackResult {
    std::vector<LAC> selected;  /*!< Selected LACs */
    int32_t total_gain;         /*!< Total size gain */
    double total_error;         /*!< Total error */
};

/*! \brief Select LACs using 0-1 knapsack optimization.
 *
 *  Maximizes total size gain while keeping total error within budget.
 *  Uses dynamic programming with discretized error values.
 *
 *  \param lacs Available LACs to select from
 *  \param error_budget Maximum total error allowed
 *  \param error_granularity Discretization granularity (default 0.001)
 *  \return Selected LACs with total gain and error
 */
KnapsackResult knapsack_select(std::vector<LAC> const& lacs, double error_budget,
                               double error_granularity = 0.001);

/*! \brief Greedy LAC selection by benefit ratio.
 *
 *  Selects LACs in order of benefit ratio (gain/error) until budget exhausted.
 *  Faster than knapsack but may not find optimal solution.
 *
 *  \param lacs Available LACs to select from
 *  \param error_budget Maximum total error allowed
 *  \return Selected LACs with total gain and error
 */
KnapsackResult greedy_select(std::vector<LAC> const& lacs, double error_budget);

} // namespace lut_synth::approximate
