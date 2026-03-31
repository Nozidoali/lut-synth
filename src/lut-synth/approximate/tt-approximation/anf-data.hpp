#pragma once

#include <cstdint>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>

namespace lut_synth::approximate {

/*! \brief ANF coefficient data for a set of truth tables.
 *
 *  Stores the algebraic normal form coefficients and indices of
 *  degree->=2 monomials for each output function.
 */
struct ANFData {
    std::vector<std::vector<bool>> coefficients;
    std::vector<uint32_t> high_degree_indices;
    uint32_t num_vars;
    uint32_t num_minterms;
};

/*! \brief ANF data for cofactor-based decomposition.
 *
 *  Stores per-cofactor ANF coefficients and parent relationships
 *  for the SS cofactor product tree ILP formulation.
 */
struct CofactorANFData {
    uint32_t k;
    uint32_t n;
    uint32_t m;
    uint32_t n_rem;
    uint32_t num_cofactors;
    uint32_t num_anf_minterms;

    std::vector<uint32_t> high_degree_indices;
    std::vector<std::vector<bool>> original_coeff;
    std::vector<int32_t> parent_hd_idx;
};

/*! \brief Compute ANF coefficients and high-degree indices from truth tables.
 *  \param tts Output truth tables (all must have the same number of variables)
 */
ANFData compute_anf_data(std::vector<kitty::dynamic_truth_table> const& tts);

/*! \brief Count active degree->=2 ANF monomials across all outputs. */
uint32_t count_high_degree_active(ANFData const& data);

/*! \brief Enumerate all subsets of a bitmask (including the mask itself). */
std::vector<uint32_t> enumerate_subsets(uint32_t mask);

/*! \brief Compute cofactor ANF data for a given cofactor split k.
 *
 *  Splits each truth table into 2^k cofactors on the lowest k variables,
 *  computes the ANF of each cofactor, and records parent relationships
 *  among high-degree monomials.
 *
 *  \param tts Output truth tables
 *  \param k_val Number of variables used for cofactor split
 */
CofactorANFData compute_cofactor_anf_data(
    std::vector<kitty::dynamic_truth_table> const& tts, uint32_t k_val);

} // namespace lut_synth::approximate
