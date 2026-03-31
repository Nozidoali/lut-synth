#pragma once

/*! \file approximate.hpp
 *  \brief Public API for approximate logic synthesis.
 *
 *  This module provides the ResubALS algorithm for approximate logic synthesis
 *  on XAG networks. The algorithm uses local approximate changes (LACs) to
 *  reduce circuit size while maintaining error bounds.
 */

#include "lut-synth/approximate/resynthesis/error-estimator.hpp"
#include "lut-synth/approximate/resynthesis/knapsack.hpp"
#include "lut-synth/approximate/resynthesis/lac-manager.hpp"
#include "lut-synth/approximate/resynthesis/lac.hpp"
#include "lut-synth/approximate/resynthesis/resubals.hpp"
#include "lut-synth/approximate/resynthesis/simulation-estimator.hpp"
#include "lut-synth/approximate/resynthesis/vecbee-estimator.hpp"

namespace lut_synth::approximate {

/*! \brief Convenience function for approximate synthesis with default params.
 *  \param ntk Input XAG network
 *  \param error_bound Maximum error bound (default 0.05)
 *  \return Approximated XAG network
 */
inline mockturtle::xag_network approximate_synthesis(mockturtle::xag_network const& ntk,
                                                      double error_bound = 0.05) {
    ResubALSParams params;
    params.error_bound = error_bound;
    return resubals(ntk, params).network;
}

} // namespace lut_synth::approximate
