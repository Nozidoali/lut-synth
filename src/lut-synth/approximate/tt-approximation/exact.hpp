#pragma once

#include <cstdint>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>

namespace lut_synth::approximate {

/*! \brief Result from exact minimum-AND synthesis evaluation. */
struct ExactCostResult {
    uint32_t and_count = 0;         /*!< Minimum AND count from exact synthesis */
    uint32_t xor_count = 0;         /*!< XOR count in the synthesized XAG */
    uint64_t synthesis_time_ms = 0; /*!< SAT solving time */
    bool solved = false;            /*!< True if exact synthesis succeeded */
};

/*! \brief Compute exact minimum AND cost for truth tables.
 *
 *  Uses mockturtle exact_mc_synthesis (SAT-based with CEGAR) per output
 *  to find the minimum multiplicative complexity. Sums per-output minimum
 *  AND counts (per-output optimal, not jointly optimal).
 *
 *  \param tts Output truth tables (max 4 variables)
 *  \param conflict_limit SAT solver conflict limit (0 = unlimited)
 *  \return Result with minimum AND count and synthesis statistics
 */
ExactCostResult compute_exact_cost(
    std::vector<kitty::dynamic_truth_table> const& tts,
    uint32_t conflict_limit = 0);

} // namespace lut_synth::approximate
