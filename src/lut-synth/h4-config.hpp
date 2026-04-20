#pragma once

#include <cassert>
#include <cstdint>

namespace lut_synth {

/*! \brief Ceiling log2. Requires n >= 2. */
inline uint32_t ceil_log2(uint32_t n) {
    assert(n >= 2);
    uint32_t bits = 0;
    uint32_t v = n - 1;
    while (v > 0) {
        ++bits;
        v >>= 1;
    }
    return bits;
}

/*! \brief H4 QROM locked-bit prefix width.
 *
 *  For the PrepareTHC H4 register layout [theta=1, alt_theta=1,
 *  alt_mu=ceil(log2 rank), alt_nu=ceil(log2 rank), keep=precision],
 *  the bits corresponding to theta/alt_theta/alt_mu/alt_nu are held
 *  exact (integer-distance meaningless there). Only the trailing
 *  precision bits of keep are eligible for approximation.
 */
inline uint32_t compute_h4_locked_bits(uint32_t rank) {
    uint32_t bw_mu = ceil_log2(rank);
    return 2 + 2 * bw_mu;
}

} // namespace lut_synth
