#include "lut-synth/approximate/tt-approximation/ilp-common.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#ifdef ENABLE_GUROBI
#include <gurobi_c++.h>
#endif

namespace lut_synth::approximate {

double bit_weight_for_output(uint32_t output_index, uint32_t m,
                             std::vector<uint32_t> const& register_bitsizes) {
    if (register_bitsizes.empty()) {
        return static_cast<double>(1u << (m - 1 - output_index));
    }
    uint32_t offset = 0;
    for (uint32_t bw : register_bitsizes) {
        assert(bw < 32);
        if (output_index < offset + bw) {
            uint32_t j = output_index - offset;
            return static_cast<double>(1u << (bw - 1 - j));
        }
        offset += bw;
    }
    assert(false);
    return 1.0;
}

double max_error_per_minterm(uint32_t m,
                             std::vector<uint32_t> const& register_bitsizes) {
    if (register_bitsizes.empty()) {
        return static_cast<double>((1u << m) - 1);
    }
    double total = 0.0;
    for (uint32_t bw : register_bitsizes) {
        assert(bw < 32);
        total += static_cast<double>((1u << bw) - 1);
    }
    return total;
}

PruningMask compute_pruning_mask(
    ANFData const& anf,
    uint32_t m,
    std::vector<double> const& weights,
    double error_bound,
    std::vector<uint32_t> const& register_bitsizes) {

    uint32_t num_minterms = anf.num_minterms;
    uint32_t num_hd = anf.high_degree_indices.size();

    PruningMask mask;
    mask.flip_pruned.assign(m, std::vector<bool>(num_minterms, false));
    mask.parity_active.assign(m, std::vector<bool>(num_hd, true));
    mask.stats.total_flip_vars = m * num_minterms;
    mask.stats.total_parity_constraints = m * num_hd;

    for (uint32_t i = 0; i < m; ++i) {
        double bit_weight = bit_weight_for_output(i, m, register_bitsizes);
        for (uint32_t x = 0; x < num_minterms; ++x) {
            if (weights[x] * bit_weight > error_bound) {
                mask.flip_pruned[i][x] = true;
                ++mask.stats.pruned_flip_vars;
            }
        }
    }

    for (uint32_t i = 0; i < m; ++i) {
        for (uint32_t j = 0; j < num_hd; ++j) {
            uint32_t S = anf.high_degree_indices[j];

            bool any_active = false;
            for (uint32_t out = 0; out < m; ++out) {
                if (anf.coefficients[out][S]) {
                    any_active = true;
                    break;
                }
            }
            if (!any_active) {
                mask.parity_active[i][j] = false;
                ++mask.stats.pruned_parity_constraints;
                continue;
            }

            bool all_subsets_pruned = true;
            uint32_t sub = S;
            do {
                if (!mask.flip_pruned[i][sub]) {
                    all_subsets_pruned = false;
                    break;
                }
                sub = (sub - 1) & S;
            } while (sub != S);

            if (all_subsets_pruned) {
                mask.parity_active[i][j] = false;
                ++mask.stats.pruned_parity_constraints;
            }
        }
    }

    return mask;
}

#ifdef ENABLE_GUROBI

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
    bool use_named_constraints) {

    if (!register_bitsizes.empty()) {
        uint32_t total = 0;
        for (uint32_t bw : register_bitsizes) total += bw;
        assert(total == m);
    }

    if (register_bitsizes.empty()) {
        for (uint32_t x = 0; x < num_minterms; ++x) {
            GRBLinExpr delta = 0;
            for (uint32_t i = 0; i < m; ++i) {
                int32_t coeff = (1 - 2 * f_values[i][x]) * (1 << (m - 1 - i));
                delta += coeff * d_vars[i][x];
            }
            if (use_named_constraints) {
                model.addConstr(e_vars[x] >= delta, "abs_pos_" + std::to_string(x));
                model.addConstr(e_vars[x] >= -delta, "abs_neg_" + std::to_string(x));
            } else {
                model.addConstr(e_vars[x] >= delta);
                model.addConstr(e_vars[x] >= -delta);
            }
        }
    } else {
        uint32_t R = register_bitsizes.size();
        std::vector<std::vector<GRBVar>> er_vars(R);
        for (uint32_t r = 0; r < R; ++r) {
            er_vars[r].reserve(num_minterms);
            double ub = static_cast<double>((1u << register_bitsizes[r]) - 1);
            for (uint32_t x = 0; x < num_minterms; ++x) {
                if (use_named_constraints) {
                    er_vars[r].push_back(model.addVar(0.0, ub, 0.0, GRB_CONTINUOUS,
                        "er_" + std::to_string(r) + "_" + std::to_string(x)));
                } else {
                    er_vars[r].push_back(model.addVar(0.0, ub, 0.0, GRB_CONTINUOUS));
                }
            }
        }

        uint32_t bit_offset = 0;
        for (uint32_t r = 0; r < R; ++r) {
            uint32_t bw = register_bitsizes[r];
            for (uint32_t x = 0; x < num_minterms; ++x) {
                GRBLinExpr delta_r = 0;
                for (uint32_t j = 0; j < bw; ++j) {
                    uint32_t i = bit_offset + j;
                    int32_t coeff = (1 - 2 * f_values[i][x]) * (1 << (bw - 1 - j));
                    delta_r += coeff * d_vars[i][x];
                }
                if (use_named_constraints) {
                    model.addConstr(er_vars[r][x] >= delta_r,
                        "abs_pos_r" + std::to_string(r) + "_" + std::to_string(x));
                    model.addConstr(er_vars[r][x] >= -delta_r,
                        "abs_neg_r" + std::to_string(r) + "_" + std::to_string(x));
                } else {
                    model.addConstr(er_vars[r][x] >= delta_r);
                    model.addConstr(er_vars[r][x] >= -delta_r);
                }
            }
            bit_offset += bw;
        }

        for (uint32_t x = 0; x < num_minterms; ++x) {
            GRBLinExpr sum_er = 0;
            for (uint32_t r = 0; r < R; ++r) {
                sum_er += er_vars[r][x];
            }
            if (use_named_constraints) {
                model.addConstr(e_vars[x] >= sum_er,
                    "e_sum_" + std::to_string(x));
            } else {
                model.addConstr(e_vars[x] >= sum_er);
            }
        }
    }

    GRBLinExpr error_sum = 0;
    for (uint32_t x = 0; x < num_minterms; ++x) {
        error_sum += weights[x] * e_vars[x];
    }
    if (use_named_constraints) {
        model.addConstr(error_sum <= error_bound, "error_bound");
    } else {
        model.addConstr(error_sum <= error_bound);
    }
}

#endif

} // namespace lut_synth::approximate
