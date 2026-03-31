#include "lut-synth/synthesis/variable-selection.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <random>

#include <kitty/operations.hpp>
#include <kitty/operators.hpp>

namespace lut_synth {

namespace {

uint32_t count_anf_terms(kitty::dynamic_truth_table const &tt) {
    kitty::dynamic_truth_table anf = tt;
    const uint32_t num_vars = tt.num_vars();
    for (uint32_t i = 0; i < num_vars; ++i) {
        kitty::dynamic_truth_table cof0 = kitty::cofactor0(anf, i);
        kitty::dynamic_truth_table cof1 = kitty::cofactor1(anf, i);
        kitty::cofactor0_inplace(anf, i);
        for (uint64_t j = 0; j < anf.num_bits(); ++j) {
            if (kitty::get_bit(cof0, j) != kitty::get_bit(cof1, j)) {
                kitty::set_bit(anf, j);
            } else {
                kitty::clear_bit(anf, j);
            }
        }
    }
    return static_cast<uint32_t>(kitty::count_ones(anf));
}

uint32_t count_support(kitty::dynamic_truth_table const &tt) {
    uint32_t support = 0;
    for (uint32_t i = 0; i < tt.num_vars(); ++i) {
        kitty::dynamic_truth_table cof0 = kitty::cofactor0(tt, i);
        kitty::dynamic_truth_table cof1 = kitty::cofactor1(tt, i);
        if (cof0 != cof1) {
            ++support;
        }
    }
    return support;
}

std::vector<uint32_t> argsort_descending(std::vector<double> const &scores) {
    std::vector<uint32_t> indices(scores.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(),
              [&scores](uint32_t a, uint32_t b) { return scores[a] > scores[b]; });
    return indices;
}

std::vector<uint32_t> argsort_ascending(std::vector<double> const &scores) {
    std::vector<uint32_t> indices(scores.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(),
              [&scores](uint32_t a, uint32_t b) { return scores[a] < scores[b]; });
    return indices;
}

} // namespace

double compute_influence(kitty::dynamic_truth_table const &tt, uint32_t var_index) {
    kitty::dynamic_truth_table cof0 = kitty::cofactor0(tt, var_index);
    kitty::dynamic_truth_table cof1 = kitty::cofactor1(tt, var_index);

    uint32_t flips = 0;
    for (uint64_t i = 0; i < cof0.num_bits(); ++i) {
        if (kitty::get_bit(cof0, i) != kitty::get_bit(cof1, i)) {
            ++flips;
        }
    }

    return static_cast<double>(flips) / static_cast<double>(cof0.num_bits());
}

std::vector<double> compute_all_influences(kitty::dynamic_truth_table const &tt) {
    std::vector<double> influences;
    influences.reserve(tt.num_vars());
    for (uint32_t i = 0; i < tt.num_vars(); ++i) {
        influences.push_back(compute_influence(tt, i));
    }
    return influences;
}

uint32_t estimate_cofactor_cost(kitty::dynamic_truth_table const &tt, uint32_t var_index) {
    kitty::dynamic_truth_table cof0 = kitty::cofactor0(tt, var_index);
    kitty::dynamic_truth_table cof1 = kitty::cofactor1(tt, var_index);
    return count_anf_terms(cof0) + count_anf_terms(cof1);
}

double compute_entropy(kitty::dynamic_truth_table const &tt, uint32_t var_index) {
    kitty::dynamic_truth_table cof0 = kitty::cofactor0(tt, var_index);
    kitty::dynamic_truth_table cof1 = kitty::cofactor1(tt, var_index);

    uint32_t ones_when_0 = static_cast<uint32_t>(kitty::count_ones(cof0));
    uint32_t ones_when_1 = static_cast<uint32_t>(kitty::count_ones(cof1));
    uint32_t total = static_cast<uint32_t>(cof0.num_bits());

    double p0 = static_cast<double>(ones_when_0) / static_cast<double>(total);
    double p1 = static_cast<double>(ones_when_1) / static_cast<double>(total);

    double avg_p = (p0 + p1) / 2.0;
    return 1.0 - std::abs(avg_p - 0.5) * 2.0;
}

uint32_t estimate_support_size(kitty::dynamic_truth_table const &tt, uint32_t var_index) {
    kitty::dynamic_truth_table cof0 = kitty::cofactor0(tt, var_index);
    kitty::dynamic_truth_table cof1 = kitty::cofactor1(tt, var_index);
    return count_support(cof0) + count_support(cof1);
}

VariableRanking rank_variables(std::vector<kitty::dynamic_truth_table> const &tts,
                               VariableSelectionMethod method) {
    assert(!tts.empty());
    const uint32_t num_vars = tts.front().num_vars();

    VariableRanking ranking;
    ranking.scores.resize(num_vars, 0.0);

    switch (method) {
    case VariableSelectionMethod::Positional: {
        ranking.order.resize(num_vars);
        std::iota(ranking.order.begin(), ranking.order.end(), 0);
        for (uint32_t i = 0; i < num_vars; ++i) {
            ranking.scores[i] = static_cast<double>(num_vars - i);
        }
        break;
    }

    case VariableSelectionMethod::Random: {
        ranking.order.resize(num_vars);
        std::iota(ranking.order.begin(), ranking.order.end(), 0);
        for (uint32_t i = 0; i < num_vars; ++i) {
            ranking.scores[i] = 0.0;
        }
        break;
    }

    case VariableSelectionMethod::Influence: {
        for (auto const &tt : tts) {
            std::vector<double> influences = compute_all_influences(tt);
            for (uint32_t i = 0; i < num_vars; ++i) {
                ranking.scores[i] += influences[i];
            }
        }
        ranking.order = argsort_descending(ranking.scores);
        break;
    }

    case VariableSelectionMethod::CofactorCost: {
        for (auto const &tt : tts) {
            for (uint32_t i = 0; i < num_vars; ++i) {
                ranking.scores[i] += estimate_cofactor_cost(tt, i);
            }
        }
        ranking.order = argsort_ascending(ranking.scores);
        break;
    }

    case VariableSelectionMethod::Support: {
        for (auto const &tt : tts) {
            for (uint32_t i = 0; i < num_vars; ++i) {
                ranking.scores[i] += estimate_support_size(tt, i);
            }
        }
        ranking.order = argsort_ascending(ranking.scores);
        break;
    }

    case VariableSelectionMethod::Entropy: {
        for (auto const &tt : tts) {
            for (uint32_t i = 0; i < num_vars; ++i) {
                ranking.scores[i] += compute_entropy(tt, i);
            }
        }
        ranking.order = argsort_descending(ranking.scores);
        break;
    }
    }

    return ranking;
}

std::vector<uint32_t> select_variables(std::vector<kitty::dynamic_truth_table> const &tts,
                                       uint32_t /*k*/, VariableSelectionParams const &params) {
    assert(!tts.empty());
    const uint32_t num_vars = tts.front().num_vars();

    if (params.method == VariableSelectionMethod::Random) {
        std::vector<uint32_t> order(num_vars);
        std::iota(order.begin(), order.end(), 0);
        std::mt19937_64 rng(params.seed);
        std::shuffle(order.begin(), order.end(), rng);
        return order;
    }

    VariableRanking ranking = rank_variables(tts, params.method);
    return ranking.order;
}

VariableSelectionMethod parse_variable_selection_method(std::string const &name) {
    if (name == "positional") return VariableSelectionMethod::Positional;
    if (name == "random") return VariableSelectionMethod::Random;
    if (name == "influence") return VariableSelectionMethod::Influence;
    if (name == "cofactor") return VariableSelectionMethod::CofactorCost;
    if (name == "support") return VariableSelectionMethod::Support;
    if (name == "entropy") return VariableSelectionMethod::Entropy;
    return VariableSelectionMethod::Positional;
}

std::string to_string(VariableSelectionMethod method) {
    switch (method) {
    case VariableSelectionMethod::Positional: return "positional";
    case VariableSelectionMethod::Random: return "random";
    case VariableSelectionMethod::Influence: return "influence";
    case VariableSelectionMethod::CofactorCost: return "cofactor";
    case VariableSelectionMethod::Support: return "support";
    case VariableSelectionMethod::Entropy: return "entropy";
    }
    return "unknown";
}

} // namespace lut_synth
