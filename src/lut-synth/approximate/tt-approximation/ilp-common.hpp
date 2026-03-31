#pragma once

#include <cstdint>
#include <vector>

#include "lut-synth/approximate/tt-approximation/anf-data.hpp"
#include "lut-synth/approximate/tt-approximation/tt-approximation.hpp"

#ifdef ENABLE_GUROBI
#include <gurobi_c++.h>
#endif

namespace lut_synth::approximate {

/*! \brief Compute the integer weight of an output bit position.
 *
 *  Returns 2^(bw-1-j) where j is the position within its register.
 *  When register_bitsizes is empty, treats all outputs as a single register.
 */
double bit_weight_for_output(uint32_t output_index, uint32_t m,
                             std::vector<uint32_t> const& register_bitsizes);

/*! \brief Compute the maximum possible error per minterm.
 *
 *  Returns the sum of (2^bw - 1) over all registers, or (2^m - 1)
 *  when register_bitsizes is empty.
 */
double max_error_per_minterm(uint32_t m,
                             std::vector<uint32_t> const& register_bitsizes);

/*! \brief Pruning mask for ILP variable and constraint reduction.
 *
 *  Identifies flip variables and parity constraints that can be
 *  fixed or skipped without affecting optimality.
 */
struct PruningMask {
    std::vector<std::vector<bool>> flip_pruned;
    std::vector<std::vector<bool>> parity_active;
    PruningStats stats;
};

/*! \brief Compute pruning mask for ILP formulation.
 *
 *  Prunes flip variables whose single-bit error exceeds the bound,
 *  and parity constraints whose subsets are all pruned or inactive.
 */
PruningMask compute_pruning_mask(
    ANFData const& anf,
    uint32_t m,
    std::vector<double> const& weights,
    double error_bound,
    std::vector<uint32_t> const& register_bitsizes);

#ifdef ENABLE_GUROBI

/*! \brief Add error constraints to a Gurobi ILP model.
 *
 *  Creates absolute-value error variables and constrains the weighted
 *  sum of per-minterm errors to stay within the error bound.
 *  Supports both single-register and per-register error metrics.
 */
void add_error_constraints(
    GRBModel& model,
    std::vector<GRBVar>& e_vars,
    std::vector<std::vector<GRBVar>> const& d_vars,
    std::vector<std::vector<int32_t>> const& f_values,
    uint32_t m,
    uint32_t num_minterms,
    std::vector<double> const& weights,
    double error_bound,
    std::vector<uint32_t> const& register_bitsizes,
    bool use_named_constraints);

#endif

} // namespace lut_synth::approximate
