#pragma once

#include <cstdint>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>

#include "lut-synth/error.hpp"

namespace lut_synth::approximate {

/*! \brief Objective type for ILP-based truth table approximation. */
enum class TTApproxObjective {
    GlobalANF,    /*!< Sum of per-output degree->=2 ANF monomials (proxy) */
    SSCofactor    /*!< Exact SS AND cost with cofactor product tree model */
};

/*! \brief Parameters for ILP-based truth table approximation. */
struct TTApproxParams {
    double error_bound = 1.0;           /*!< Max weighted mean integer error */
    std::vector<double> weights;        /*!< Per-input weights (empty = uniform) */
    double time_limit = 60.0;           /*!< Gurobi time limit (seconds) */
    bool enable_pruning = true;         /*!< Enable heuristic variable pruning */
    bool verbose = false;               /*!< Enable verbose output */
    TTApproxObjective objective = TTApproxObjective::SSCofactor; /*!< Objective type */
    int fixed_k = -1;                   /*!< Fix k for cofactor split (-1 = try all) */
};

/*! \brief Statistics from heuristic variable pruning. */
struct PruningStats {
    uint32_t total_flip_vars = 0;           /*!< Total d_{i,x} variables */
    uint32_t pruned_flip_vars = 0;          /*!< Flip variables fixed to 0 */
    uint32_t total_parity_constraints = 0;  /*!< Total parity constraints */
    uint32_t pruned_parity_constraints = 0; /*!< Parity constraints skipped */
};

/*! \brief Result from ILP-based truth table approximation. */
struct TTApproxResult {
    std::vector<kitty::dynamic_truth_table> approx_tts;  /*!< Approximated truth tables */
    uint32_t bits_flipped = 0;          /*!< Total number of bits flipped */
    uint32_t monomials_before = 0;      /*!< Degree->=2 ANF monomials before */
    uint32_t monomials_after = 0;       /*!< Degree->=2 ANF monomials after */
    lut_synth::IntegerError error;      /*!< Integer error statistics */
    PruningStats pruning;               /*!< Heuristic pruning statistics */
    bool solved = false;                /*!< True if ILP found feasible solution */
    int best_k = 0;                     /*!< k that minimized SS AND count */
    uint32_t ss_and_estimate = 0;       /*!< ILP-predicted SS AND count */
    uint32_t ss_and_actual = 0;         /*!< Actual SS AND count from synthesis */
};

/*! \brief Approximate truth tables by ILP-optimized bit flipping.
 *
 *  Formulates an ILP that selects which output bits to flip to minimize
 *  the total number of degree->=2 ANF monomials (proxy for AND gates in
 *  SS synthesis), subject to a weighted mean integer error bound.
 *
 *  Requires Gurobi. Returns solved=false if Gurobi is not available.
 *
 *  \param exact_tts Exact output truth tables (MSB first)
 *  \param params Approximation parameters
 *  \return Result with approximated truth tables and statistics
 */
TTApproxResult approximate_truth_table_ilp(
    std::vector<kitty::dynamic_truth_table> const& exact_tts,
    TTApproxParams const& params = {});

/*! \brief Count degree->=2 ANF monomials across all outputs.
 *  \param tts Output truth tables
 *  \return Total number of degree->=2 monomials
 */
uint32_t count_high_degree_monomials(
    std::vector<kitty::dynamic_truth_table> const& tts);

} // namespace lut_synth::approximate
