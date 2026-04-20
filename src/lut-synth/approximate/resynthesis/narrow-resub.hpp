#pragma once

#include <cstdint>
#include <vector>

#include <mockturtle/networks/xag.hpp>

namespace lut_synth::approximate {

/*! \brief Parameters for AND-targeted narrow approximate resubstitution.
 *
 *  Unlike ResubALS, this heuristic does not enumerate a divisor window.
 *  For every AND gate in the XAG it only tries the five local substitutions
 *  using the gate's own fanins: constant 0/1, either fanin directly, and
 *  XOR of the two fanins. Intended for use cases (e.g., H4 QROM keep
 *  register) where the caller wants a fast, solver-free pass that
 *  specifically attacks AND count.
 */
struct NarrowResubParams {
    uint32_t num_patterns = 102400;    /*!< Simulation patterns */
    uint32_t seed = 0;                 /*!< Simulation seed */
    double error_bound = 0.05;         /*!< Max accumulated error (integer mean) */
    std::vector<double> weights;       /*!< Per-input weights (empty = uniform) */
    bool try_const0 = true;            /*!< Consider replacing AND with constant 0 */
    bool try_const1 = true;            /*!< Consider replacing AND with constant 1 */
    bool try_fanin = true;             /*!< Consider replacing AND with either fanin */
    bool try_xor = true;               /*!< Consider replacing AND with XOR of fanins */
    uint32_t max_iterations = 0;       /*!< 0 = unlimited */
    uint32_t max_integer_error_per_pattern = 0;  /*!< Reject LACs whose max single-pattern integer error exceeds this (0 = disabled) */
    std::vector<bool> locked_outputs;  /*!< Per-PO lock; ANDs reachable from any locked PO are skipped (empty = no lock) */
};

/*! \brief Statistics from a narrow AND-resub pass. */
struct NarrowResubStats {
    uint32_t original_size = 0;        /*!< Gates before */
    uint32_t final_size = 0;           /*!< Gates after */
    uint32_t and_before = 0;           /*!< AND count before */
    uint32_t and_after = 0;            /*!< AND count after */
    uint32_t lacs_applied = 0;         /*!< Number of substitutions applied */
    double accumulated_error = 0.0;    /*!< Total weighted integer error consumed */
    uint32_t and_locked = 0;           /*!< AND gates skipped because they feed a locked PO */
    uint32_t lacs_rejected_by_cap = 0; /*!< Candidate LACs filtered out by max_integer_error_per_pattern */
};

/*! \brief Result of a narrow AND-resub pass. */
struct NarrowResubResult {
    mockturtle::xag_network network;   /*!< Approximated network */
    NarrowResubStats stats;            /*!< Execution statistics */
};

/*! \brief Run AND-targeted narrow approximate resubstitution.
 *
 *  Greedy loop: re-simulate, score every AND gate's five local candidates
 *  by (size_gain / error_delta), apply the best one that still fits in
 *  the remaining error budget, repeat until no candidate fits.
 *
 *  \param ntk Input XAG network
 *  \param params Pass parameters
 *  \return Approximated network and statistics
 */
NarrowResubResult narrow_and_resub(
    mockturtle::xag_network const& ntk,
    NarrowResubParams const& params = {});

} // namespace lut_synth::approximate
