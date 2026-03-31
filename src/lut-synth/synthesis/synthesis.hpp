#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>
#include <mockturtle/networks/xag.hpp>

#include "lut-synth/synthesis/variable-selection.hpp"
#include "lut-synth/synthesis/xag-synthesizer.hpp"

namespace lut_synth { class TruthTable; }

namespace lut_synth {

/*! \brief Validate truth tables have consistent variable count.
 *  \param tts Vector of truth tables
 *  \param function_name Function name for debugging
 *  \return Number of variables
 */
uint32_t check_tts(std::vector<kitty::dynamic_truth_table> const &tts,
                   std::string_view function_name);

/*! \brief Synthesize XAG network from truth tables.
 *
 *  Main entry point for truth table to XAG synthesis. Supports multiple
 *  synthesis methods with different quality/speed trade-offs.
 *
 *  Available methods:
 *  - **ss** (default): Select-swap with MUX tree decomposition.
 *    Best quality for most functions. Uses Shannon decomposition with
 *    automatic k selection to minimize AND gates.
 *  - **davio**: Positive Davio decomposition. Fast but produces more ANDs.
 *    Good for functions with XOR structure.
 *  - **ac**: Affine classification. Groups inputs by affine equivalence.
 *    Quality depends on ac_max_lut parameter.
 *  - **dsd**: Disjoint support decomposition. Exploits structural properties.
 *    Can produce compact results for functions with disjoint support.
 *  - **exact**: SAT-based exact synthesis. Finds minimum AND count but
 *    exponential runtime. Only practical for small functions (n <= 6).
 *
 *  Algorithm (for ss method):
 *  1. Try all k values from 1 to n-1 (or use specified k)
 *  2. For each k: split variables into k selector bits and n-k ANF bits
 *  3. Build MUX tree from selector bits
 *  4. Compute ANF (algebraic normal form) for each cofactor
 *  5. Select k with minimum AND count
 *
 *  \param tts Vector of output truth tables (all must have same num_vars)
 *  \param method Synthesis method: "ss", "davio", "ac", "dsd", "exact"
 *  \param ac_max_lut Maximum LUT size for AC method (default: 4)
 *  \param ss_k Shannon parameter for ss method (-1 = auto-select best k)
 *  \return Synthesized XAG network minimizing AND (multiplicative complexity)
 *
 *  Example:
 *  ```cpp
 *  std::vector<kitty::dynamic_truth_table> tts = {...};
 *  auto xag = lut_synth::synthesize_xag_network(tts, "ss");  // Best quality
 *  auto xag_fast = lut_synth::synthesize_xag_network(tts, "davio");  // Faster
 *  ```
 */
mockturtle::xag_network synthesize_xag_network(std::vector<kitty::dynamic_truth_table> const &tts,
                                               std::string_view method = "ss",
                                               uint32_t ac_max_lut = 4, int ss_k = -1);

/*! \brief Synthesize XAG network from TruthTable with don't-care support.
 *
 *  Overload that accepts a TruthTable object which may contain don't-care
 *  conditions. When don't-cares are present and method is "ss", uses
 *  specialized synthesis that exploits don't-cares for better optimization.
 *
 *  Don't-care exploitation:
 *  - TruthTable stores ON-set and OFF-set separately
 *  - Bits not in either set are don't-cares
 *  - Synthesis can choose 0 or 1 for don't-cares to minimize AND count
 *
 *  \param tt Input truth table (may contain don't-cares)
 *  \param method Synthesis method (don't-cares only supported for "ss")
 *  \param ac_max_lut Maximum LUT size for AC method
 *  \param ss_k Shannon parameter for ss method (-1 = auto)
 *  \param disable_dont_care If true, ignore don't-cares and treat as zeros
 *  \return Synthesized XAG network
 *
 *  Example:
 *  ```cpp
 *  TruthTable tt(on_set, off_set);  // Has don't-cares
 *  auto xag = lut_synth::synthesize_xag_network(tt, "ss");  // Exploits DCs
 *  auto xag_no_dc = lut_synth::synthesize_xag_network(tt, "ss", 4, -1, true);
 *  ```
 */
mockturtle::xag_network synthesize_xag_network(lut_synth::TruthTable const &tt,
                                               std::string_view method = "ss",
                                               uint32_t ac_max_lut = 4, int ss_k = -1,
                                               bool disable_dont_care = false);

/*! \brief Synthesize XAG using Shannon with don't-care sets.
 *  \param on ON-set truth tables
 *  \param off OFF-set truth tables
 *  \param k Shannon parameter (-1 for auto)
 *  \param num_random_starts Number of random variable orderings to try (1 = no multi-start)
 *  \param seed Random seed for multi-start
 *  \return Synthesized XAG network
 */
mockturtle::xag_network synthesize_selectswap_dontcare(std::vector<kitty::dynamic_truth_table> const &on,
                                        std::vector<kitty::dynamic_truth_table> const &off,
                                        int k = -1, uint32_t num_random_starts = 1,
                                        uint64_t seed = 0);

/*! \brief Synthesize XAG using select-swap (SS) method.
 *  \param tts Vector of output truth tables
 *  \param k Shannon parameter (-1 for auto)
 *  \param num_random_starts Number of random variable orderings to try (1 = no multi-start)
 *  \param seed Random seed for multi-start
 *  \return Synthesized XAG network
 */
mockturtle::xag_network synthesize_selectswap(std::vector<kitty::dynamic_truth_table> const &tts,
                                       int k = -1, uint32_t num_random_starts = 1,
                                       uint64_t seed = 0);

/*! \brief Internal helpers for sum-of-products synthesis. */
namespace ss_detail {

/*! \brief Iterate over cofactor entries for k-variable Shannon split. */
template <typename Fn>
void for_each_cofactor_entry(int k, int num_vars, int m, Fn&& fn) {
    int const remaining_vars = num_vars - k;
    int const num_cofactors = 1 << k;
    for (int cof_idx = 0; cof_idx < num_cofactors; ++cof_idx) {
        for (int assignment = 0; assignment < (1 << remaining_vars); ++assignment) {
            int const original_index = cof_idx | (assignment << k);
            for (int output = 0; output < m; ++output) {
                fn(cof_idx, assignment, output, original_index);
            }
        }
    }
}

/*! \brief Convert truth table to algebraic normal form. */
kitty::dynamic_truth_table to_anf(kitty::dynamic_truth_table const &tt);

/*! \brief Build XOR tree from signals. */
mockturtle::xag_network::signal xor_tree(mockturtle::xag_network &network,
                                         std::vector<mockturtle::xag_network::signal> signals);

/*! \brief Build 2:1 multiplexer. */
mockturtle::xag_network::signal mux2(mockturtle::xag_network &network,
                                     mockturtle::xag_network::signal select,
                                     mockturtle::xag_network::signal a,
                                     mockturtle::xag_network::signal b);

/*! \brief Generate all product terms from primary inputs. */
std::vector<mockturtle::xag_network::signal>
generate_product_terms(mockturtle::xag_network &network,
                       std::vector<mockturtle::xag_network::signal> const &pis);

/*! \brief Select outputs based on ANF coefficients. */
std::vector<mockturtle::xag_network::signal>
select_outputs_from_anfs(mockturtle::xag_network &network,
                         std::vector<kitty::dynamic_truth_table> const &anfs,
                         std::vector<mockturtle::xag_network::signal> const &pis);

/*! \brief Create primary input signals. */
std::vector<mockturtle::xag_network::signal> create_pis(mockturtle::xag_network &network,
                                                        int num_vars);

/*! \brief Permute truth table variables according to order. */
kitty::dynamic_truth_table permute_variables(kitty::dynamic_truth_table const &tt,
                                             std::vector<uint32_t> const &order);

/*! \brief Permute variables of all truth tables. */
std::vector<kitty::dynamic_truth_table>
permute_all_variables(std::vector<kitty::dynamic_truth_table> const &tts,
                      std::vector<uint32_t> const &order);

/*! \brief Build MUX tree from select outputs and create primary outputs. */
void build_mux_tree(mockturtle::xag_network &network,
                    std::vector<mockturtle::xag_network::signal> const &select_outputs,
                    std::vector<mockturtle::xag_network::signal> const &select_pis,
                    int num_outputs, int k);

} // namespace ss_detail

/*! \brief Parameters for SAT-based exact synthesis. */
struct ExactSynthesisParams {
    uint32_t max_vars = 6;       /*!< Maximum variable count */
    bool use_cegar = true;       /*!< Use CEGAR approach */
    bool verbose = false;        /*!< Enable verbose output */
};

/*! \brief Statistics from exact synthesis. */
struct ExactSynthesisStats {
    uint64_t total_time_ms = 0;  /*!< Total synthesis time */
    uint32_t min_and_found = 0;  /*!< Minimum AND count found */
};

/*! \brief Synthesize minimum-AND XAG using SAT solver.
 *  \param tts Vector of output truth tables
 *  \param params Synthesis parameters
 *  \return Synthesized XAG network
 */
mockturtle::xag_network synthesize_exact(std::vector<kitty::dynamic_truth_table> const &tts,
                                          ExactSynthesisParams const &params = {});

/*! \brief Synthesize minimum-AND XAG for single output.
 *  \param tt Output truth table
 *  \param params Synthesis parameters
 *  \param stats Output statistics
 *  \return Synthesized XAG network
 */
mockturtle::xag_network synthesize_exact(kitty::dynamic_truth_table const &tt,
                                          ExactSynthesisParams const &params = {},
                                          ExactSynthesisStats *stats = nullptr);

/*! \brief Result of combinational equivalence checking. */
struct CECResult {
    bool equivalent = false;           /*!< Networks are equivalent */
    bool completed = false;            /*!< Check completed without error */
    std::string error_message;         /*!< Error message if failed */
    std::vector<bool> counter_example; /*!< Counter-example if not equivalent */
};

/*! \brief Count AND gates for sum-of-products with parameter k. */
uint32_t count_ss_ands_for_k(std::vector<kitty::dynamic_truth_table> const &tts, int k);

/*! \brief Result from SS sensitivity analysis. */
struct SSSensitivityResult {
    uint32_t best_and_count{0};       /*!< Best AND count found */
    int best_k{0};                    /*!< Best k parameter */
    std::vector<uint32_t> best_order; /*!< Best variable order */
};

/*! \brief Synthesize SS with specified variable order.
 *  \param tts Vector of truth tables
 *  \param var_order Variable permutation (var_order[i] = original index at position i)
 *  \param k Shannon parameter (-1 for auto)
 *  \return Synthesized XAG network
 */
mockturtle::xag_network synthesize_selectswap_with_order(std::vector<kitty::dynamic_truth_table> const &tts,
                                                  std::vector<uint32_t> const &var_order, int k = -1);

/*! \brief Count AND gates with specified variable order.
 *  \param tts Vector of truth tables
 *  \param var_order Variable permutation
 *  \param k Shannon parameter
 *  \return AND gate count
 */
uint32_t count_ss_ands_with_order(std::vector<kitty::dynamic_truth_table> const &tts,
                                   std::vector<uint32_t> const &var_order, int k);

/*! \brief Find best variable order using specified strategy.
 *  \param tts Vector of truth tables
 *  \param k Shannon parameter (-1 for auto)
 *  \param params Variable selection parameters
 *  \return Best result with order and AND count
 */
SSSensitivityResult find_best_variable_order(std::vector<kitty::dynamic_truth_table> const &tts,
                                              int k, VariableSelectionParams const &params);

/*! \brief Check equivalence of two XAG networks. */
CECResult check_equivalence(mockturtle::xag_network const &ntk1,
                            mockturtle::xag_network const &ntk2);

/*! \brief Check equivalence of XAG against truth tables. */
CECResult check_equivalence(mockturtle::xag_network const &ntk,
                            std::vector<kitty::dynamic_truth_table> const &tts);

} // namespace lut_synth
