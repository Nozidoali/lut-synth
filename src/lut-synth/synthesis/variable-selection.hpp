#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>

namespace lut_synth {

/*! \brief Variable selection method for SS synthesis. */
enum class VariableSelectionMethod {
    Positional,   /*!< First k variables (baseline) */
    Random,       /*!< Random permutation */
    Influence,    /*!< Rank by variable influence */
    CofactorCost, /*!< Rank by cofactor AND complexity */
    Support,      /*!< Rank by support size reduction */
    Entropy       /*!< Rank by variable entropy */
};

/*! \brief Parameters for variable selection. */
struct VariableSelectionParams {
    VariableSelectionMethod method{VariableSelectionMethod::Positional};
    uint32_t num_random_tries{10}; /*!< Tries for Random method */
    uint64_t seed{0};              /*!< Random seed */
};

/*! \brief Variable ranking with scores. */
struct VariableRanking {
    std::vector<uint32_t> order; /*!< Variable indices in priority order */
    std::vector<double> scores;  /*!< Score for each variable */
};

/*! \brief Compute influence of a variable on a truth table.
 *  \param tt Truth table
 *  \param var_index Variable index
 *  \return Influence in [0, 1]
 */
double compute_influence(kitty::dynamic_truth_table const &tt, uint32_t var_index);

/*! \brief Compute influences of all variables.
 *  \param tt Truth table
 *  \return Vector of influences for each variable
 */
std::vector<double> compute_all_influences(kitty::dynamic_truth_table const &tt);

/*! \brief Estimate cofactor AND complexity.
 *  \param tt Truth table
 *  \param var_index Variable index
 *  \return Estimated AND cost of cofactors
 */
uint32_t estimate_cofactor_cost(kitty::dynamic_truth_table const &tt, uint32_t var_index);

/*! \brief Compute variable entropy (balance).
 *  \param tt Truth table
 *  \param var_index Variable index
 *  \return Entropy score in [0, 1]
 */
double compute_entropy(kitty::dynamic_truth_table const &tt, uint32_t var_index);

/*! \brief Estimate support size after cofactoring.
 *  \param tt Truth table
 *  \param var_index Variable index
 *  \return Total support size of both cofactors
 */
uint32_t estimate_support_size(kitty::dynamic_truth_table const &tt, uint32_t var_index);

/*! \brief Rank variables by specified method.
 *  \param tts Vector of truth tables
 *  \param method Selection method
 *  \return Variable ranking with scores
 */
VariableRanking rank_variables(std::vector<kitty::dynamic_truth_table> const &tts,
                               VariableSelectionMethod method);

/*! \brief Get variable permutation for synthesis.
 *  \param tts Vector of truth tables
 *  \param k Number of variables for Shannon decomposition
 *  \param params Selection parameters
 *  \return Variable order (first k are select_pis)
 */
std::vector<uint32_t> select_variables(std::vector<kitty::dynamic_truth_table> const &tts,
                                       uint32_t k, VariableSelectionParams const &params);

/*! \brief Parse method name to enum.
 *  \param name Method name string
 *  \return Corresponding enum value
 */
VariableSelectionMethod parse_variable_selection_method(std::string const &name);

/*! \brief Convert method enum to string.
 *  \param method Method enum value
 *  \return Method name string
 */
std::string to_string(VariableSelectionMethod method);

} // namespace lut_synth
