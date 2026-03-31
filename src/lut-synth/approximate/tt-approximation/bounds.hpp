#pragma once

#include <cstdint>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>

namespace lut_synth::approximate {

/*! \brief Single select-swap bound entry for a given k. */
struct BoundEntry {
    int k;                  /*!< Swap variable count (0 = select-only) */
    uint32_t select_bound;  /*!< Alive degree->=2 monomials in product tree */
    uint32_t swap_cost;     /*!< MUX tree AND cost (exact, accounts for cofactor equality) */
    uint32_t total;         /*!< select_bound + swap_cost */
};

/*! \brief Analytical AND-gate bounds for select-swap synthesis.
 *
 *  Contains the exact AND-gate count that select-swap synthesis will
 *  produce for each value of k (number of swap variables), computed
 *  analytically from the truth table's ANF without building XAGs.
 */
struct AnalyticalBounds {
    BoundEntry select;              /*!< Select-only bound (k=0) */
    BoundEntry best_select_swap;    /*!< Best select-swap bound (k in [1, n-1]) */
    std::vector<BoundEntry> all_k;  /*!< Bounds for each k in [0, n-1] */
};

/*! \brief Compute tight analytical AND-gate bounds from truth tables.
 *
 *  For each k from 0 to n-1, computes the exact number of AND gates
 *  that select-swap synthesis will produce (matching actual synthesis
 *  after cleanup_dangling). Uses ANF analysis to count alive product
 *  terms in the monomial tree.
 *
 *  \param tts Output truth tables (all same num_vars, at least 2 vars)
 *  \return Bounds for select-only (k=0), best select-swap, and all k
 */
 AnalyticalBounds compute_analytical_bounds(
    std::vector<kitty::dynamic_truth_table> const& tts);

/*! \brief Count alive degree->=2 monomials in product term tree.
 *
 *  A monomial S (popcount >= 2) is alive if any ANF has coefficient
 *  coeff[S] = true, or if any child S' with parent S is alive.
 *  Parent relation: parent(S) = S ^ (1 << MSB(S)).
 *
 *  \param anf_coefficients Per-function ANF coefficients (size 2^num_vars each)
 *  \param num_vars Number of variables in the ANF space
 *  \return Count of alive monomials (= exact AND count for product tree)
 */
uint32_t count_alive_monomials(
    std::vector<std::vector<bool>> const& anf_coefficients,
    uint32_t num_vars);

} // namespace lut_synth::approximate
