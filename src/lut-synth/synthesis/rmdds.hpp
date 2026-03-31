#pragma once

#include <cstdint>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>
#include <mockturtle/networks/xag.hpp>

namespace lut_synth {

/*! \brief Parameters for RMDDS synthesis. */
struct RMDDSParams {
    uint32_t max_var_orderings = 16; /*!< Variable orderings to try */
    bool enable_term_reordering = true; /*!< Reorder terms by Hamming distance */
};

/*! \brief Statistics from RMDDS synthesis. */
struct RMDDSStats {
    uint32_t num_terms = 0;  /*!< Number of product terms */
    uint32_t and_count = 0;  /*!< AND gate count */
    double time_ms = 0.0;    /*!< Synthesis time */
};

/*! \brief Synthesize XAG using RMDDS method.
 *  \param tts Vector of output truth tables
 *  \param params Synthesis parameters
 *  \param stats Output statistics
 *  \return Synthesized XAG network
 */
mockturtle::xag_network synthesize_rmdds(std::vector<kitty::dynamic_truth_table> const &tts,
                                         RMDDSParams const &params = {},
                                         RMDDSStats *stats = nullptr);

/*! \brief Apply RMDDS optimization to existing XAG.
 *  \param xag Input XAG network
 *  \param params Synthesis parameters
 *  \return Optimized XAG network
 */
mockturtle::xag_network apply_rmdds_optimize(mockturtle::xag_network const &xag,
                                             RMDDSParams const &params = {});

} // namespace lut_synth
