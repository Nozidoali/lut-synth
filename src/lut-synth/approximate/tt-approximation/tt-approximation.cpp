#include "lut-synth/approximate/tt-approximation/tt-approximation.hpp"
#include "lut-synth/synthesis/synthesis.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

#include <kitty/bit_operations.hpp>
#include <kitty/dynamic_truth_table.hpp>

#ifdef ENABLE_GUROBI
#include <gurobi_c++.h>
#endif

namespace lut_synth::approximate {

namespace {

struct ANFData {
    std::vector<std::vector<bool>> coefficients;
    std::vector<uint32_t> high_degree_indices;
    uint32_t num_vars;
    uint32_t num_minterms;
};

ANFData compute_anf_data(std::vector<kitty::dynamic_truth_table> const& tts) {
    ANFData data;
    data.num_vars = tts[0].num_vars();
    data.num_minterms = 1u << data.num_vars;

    uint32_t m = tts.size();
    data.coefficients.resize(m);

    for (uint32_t i = 0; i < m; ++i) {
        kitty::dynamic_truth_table anf = ss_detail::to_anf(tts[i]);
        data.coefficients[i].resize(data.num_minterms);
        for (uint32_t s = 0; s < data.num_minterms; ++s) {
            data.coefficients[i][s] = kitty::get_bit(anf, s);
        }
    }

    for (uint32_t s = 0; s < data.num_minterms; ++s) {
        if (__builtin_popcount(s) >= 2) {
            data.high_degree_indices.push_back(s);
        }
    }

    return data;
}

uint32_t count_high_degree_active(ANFData const& data) {
    uint32_t count = 0;
    for (auto const& coeff : data.coefficients) {
        for (uint32_t s : data.high_degree_indices) {
            if (coeff[s]) {
                ++count;
            }
        }
    }
    return count;
}

std::vector<uint32_t> enumerate_subsets(uint32_t mask) {
    std::vector<uint32_t> subsets;
    uint32_t sub = mask;
    do {
        subsets.push_back(sub);
        sub = (sub - 1) & mask;
    } while (sub != mask);
    return subsets;
}

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

CofactorANFData compute_cofactor_anf_data(
    std::vector<kitty::dynamic_truth_table> const& tts, uint32_t k_val) {

    CofactorANFData data;
    data.n = tts[0].num_vars();
    data.m = tts.size();
    data.k = k_val;
    data.n_rem = data.n - k_val;
    data.num_cofactors = 1u << k_val;
    data.num_anf_minterms = 1u << data.n_rem;

    for (uint32_t s = 0; s < data.num_anf_minterms; ++s) {
        if (__builtin_popcount(s) >= 2) {
            data.high_degree_indices.push_back(s);
        }
    }

    uint32_t num_hd = data.high_degree_indices.size();
    data.original_coeff.resize(data.m);

    for (uint32_t i = 0; i < data.m; ++i) {
        data.original_coeff[i].resize(data.num_cofactors * num_hd, false);
        for (uint32_t c = 0; c < data.num_cofactors; ++c) {
            kitty::dynamic_truth_table cof_tt(data.n_rem);
            for (uint32_t t = 0; t < data.num_anf_minterms; ++t) {
                uint32_t global_idx = c | (t << k_val);
                if (kitty::get_bit(tts[i], global_idx)) {
                    kitty::set_bit(cof_tt, t);
                }
            }
            kitty::dynamic_truth_table anf = ss_detail::to_anf(cof_tt);
            for (uint32_t j = 0; j < num_hd; ++j) {
                if (kitty::get_bit(anf, data.high_degree_indices[j])) {
                    data.original_coeff[i][c * num_hd + j] = true;
                }
            }
        }
    }

    std::unordered_map<uint32_t, uint32_t> subset_to_hd_idx;
    for (uint32_t j = 0; j < num_hd; ++j) {
        subset_to_hd_idx[data.high_degree_indices[j]] = j;
    }

    data.parent_hd_idx.resize(num_hd, -1);
    for (uint32_t j = 0; j < num_hd; ++j) {
        uint32_t S = data.high_degree_indices[j];
        uint32_t msb = 31 - __builtin_clz(S);
        uint32_t parent = S ^ (1u << msb);
        if (__builtin_popcount(parent) >= 2) {
            std::unordered_map<uint32_t, uint32_t>::iterator it = subset_to_hd_idx.find(parent);
            assert(it != subset_to_hd_idx.end());
            data.parent_hd_idx[j] = static_cast<int32_t>(it->second);
        }
    }

    return data;
}

double bit_weight_for_output(uint32_t output_index, uint32_t m,
                             std::vector<uint32_t> const& register_bitsizes) {
    if (register_bitsizes.empty()) {
        return static_cast<double>(1u << (m - 1 - output_index));
    }
    uint32_t offset = 0;
    for (uint32_t bw : register_bitsizes) {
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
        total += static_cast<double>((1u << bw) - 1);
    }
    return total;
}

struct PruningMask {
    std::vector<std::vector<bool>> flip_pruned;
    std::vector<std::vector<bool>> parity_active;
    PruningStats stats;
};

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

TTApproxResult solve_ilp(
    std::vector<kitty::dynamic_truth_table> const& exact_tts,
    ANFData const& anf,
    TTApproxParams const& params) {

    uint32_t m = exact_tts.size();
    uint32_t num_minterms = anf.num_minterms;
    uint32_t num_hd = anf.high_degree_indices.size();

    std::vector<double> weights = params.weights;
    if (weights.empty()) {
        weights.assign(num_minterms, 1.0 / num_minterms);
    }

    std::vector<std::vector<int32_t>> f_values(m, std::vector<int32_t>(num_minterms));
    for (uint32_t i = 0; i < m; ++i) {
        for (uint32_t x = 0; x < num_minterms; ++x) {
            f_values[i][x] = kitty::get_bit(exact_tts[i], x) ? 1 : 0;
        }
    }

    PruningMask pmask;
    if (params.enable_pruning) {
        pmask = compute_pruning_mask(anf, m, weights, params.error_bound, params.register_bitsizes);
    } else {
        pmask.flip_pruned.assign(m, std::vector<bool>(num_minterms, false));
        pmask.parity_active.assign(m, std::vector<bool>(num_hd, true));
        pmask.stats.total_flip_vars = m * num_minterms;
        pmask.stats.total_parity_constraints = m * num_hd;
    }

    GRBEnv env(true);
    env.set(GRB_IntParam_OutputFlag, params.verbose ? 1 : 0);
    env.start();

    GRBModel model(env);
    if (params.time_limit > 0) {
        model.set(GRB_DoubleParam_TimeLimit, params.time_limit);
    }

    std::vector<std::vector<GRBVar>> d_vars(m);
    for (uint32_t i = 0; i < m; ++i) {
        d_vars[i].reserve(num_minterms);
        for (uint32_t x = 0; x < num_minterms; ++x) {
            double ub = pmask.flip_pruned[i][x] ? 0.0 : 1.0;
            d_vars[i].push_back(model.addVar(0.0, ub, 0.0, GRB_BINARY,
                "d_" + std::to_string(i) + "_" + std::to_string(x)));
        }
    }

    std::vector<std::vector<GRBVar>> beta_vars(m);
    std::vector<std::vector<GRBVar>> k_vars(m);
    for (uint32_t i = 0; i < m; ++i) {
        beta_vars[i].reserve(num_hd);
        k_vars[i].reserve(num_hd);
        for (uint32_t j = 0; j < num_hd; ++j) {
            double ub_beta = pmask.parity_active[i][j] ? 1.0 : 0.0;
            double ub_k = pmask.parity_active[i][j] ? GRB_INFINITY : 0.0;
            beta_vars[i].push_back(model.addVar(0.0, ub_beta, 0.0, GRB_BINARY,
                "b_" + std::to_string(i) + "_" + std::to_string(j)));
            k_vars[i].push_back(model.addVar(0.0, ub_k, 0.0, GRB_INTEGER,
                "k_" + std::to_string(i) + "_" + std::to_string(j)));
        }
    }

    std::vector<GRBVar> e_vars;
    e_vars.reserve(num_minterms);
    for (uint32_t x = 0; x < num_minterms; ++x) {
        double ub = max_error_per_minterm(m, params.register_bitsizes);
        e_vars.push_back(model.addVar(0.0, ub, 0.0, GRB_CONTINUOUS,
            "e_" + std::to_string(x)));
    }

    for (uint32_t i = 0; i < m; ++i) {
        for (uint32_t j = 0; j < num_hd; ++j) {
            if (!pmask.parity_active[i][j]) {
                continue;
            }
            uint32_t S = anf.high_degree_indices[j];
            GRBLinExpr sum_d = 0;
            std::vector<uint32_t> subsets = enumerate_subsets(S);
            for (uint32_t T : subsets) {
                sum_d += d_vars[i][T];
            }
            model.addConstr(sum_d == 2 * k_vars[i][j] + beta_vars[i][j],
                "parity_" + std::to_string(i) + "_" + std::to_string(j));
        }
    }

    if (params.register_bitsizes.empty()) {
        for (uint32_t x = 0; x < num_minterms; ++x) {
            GRBLinExpr delta = 0;
            for (uint32_t i = 0; i < m; ++i) {
                int32_t coeff = (1 - 2 * f_values[i][x]) * (1 << (m - 1 - i));
                delta += coeff * d_vars[i][x];
            }
            model.addConstr(e_vars[x] >= delta, "abs_pos_" + std::to_string(x));
            model.addConstr(e_vars[x] >= -delta, "abs_neg_" + std::to_string(x));
        }
    } else {
        uint32_t R = params.register_bitsizes.size();
        std::vector<std::vector<GRBVar>> er_vars(R);
        for (uint32_t r = 0; r < R; ++r) {
            er_vars[r].reserve(num_minterms);
            double ub = static_cast<double>((1u << params.register_bitsizes[r]) - 1);
            for (uint32_t x = 0; x < num_minterms; ++x) {
                er_vars[r].push_back(model.addVar(0.0, ub, 0.0, GRB_CONTINUOUS,
                    "er_" + std::to_string(r) + "_" + std::to_string(x)));
            }
        }

        uint32_t bit_offset = 0;
        for (uint32_t r = 0; r < R; ++r) {
            uint32_t bw = params.register_bitsizes[r];
            for (uint32_t x = 0; x < num_minterms; ++x) {
                GRBLinExpr delta_r = 0;
                for (uint32_t j = 0; j < bw; ++j) {
                    uint32_t i = bit_offset + j;
                    int32_t coeff = (1 - 2 * f_values[i][x]) * (1 << (bw - 1 - j));
                    delta_r += coeff * d_vars[i][x];
                }
                model.addConstr(er_vars[r][x] >= delta_r,
                    "abs_pos_r" + std::to_string(r) + "_" + std::to_string(x));
                model.addConstr(er_vars[r][x] >= -delta_r,
                    "abs_neg_r" + std::to_string(r) + "_" + std::to_string(x));
            }
            bit_offset += bw;
        }

        for (uint32_t x = 0; x < num_minterms; ++x) {
            GRBLinExpr sum_er = 0;
            for (uint32_t r = 0; r < R; ++r) {
                sum_er += er_vars[r][x];
            }
            model.addConstr(e_vars[x] >= sum_er,
                "e_sum_" + std::to_string(x));
        }
    }

    GRBLinExpr error_sum = 0;
    for (uint32_t x = 0; x < num_minterms; ++x) {
        error_sum += weights[x] * e_vars[x];
    }
    model.addConstr(error_sum <= params.error_bound, "error_bound");

    GRBLinExpr obj = 0;
    for (uint32_t i = 0; i < m; ++i) {
        for (uint32_t j = 0; j < num_hd; ++j) {
            if (anf.coefficients[i][anf.high_degree_indices[j]]) {
                obj += (1 - beta_vars[i][j]);
            } else {
                obj += beta_vars[i][j];
            }
        }
    }
    model.setObjective(obj, GRB_MINIMIZE);

    model.optimize();

    TTApproxResult result;
    result.pruning = pmask.stats;

    int sol_count = model.get(GRB_IntAttr_SolCount);
    if (sol_count == 0) {
        result.solved = false;
        result.approx_tts = exact_tts;
        return result;
    }

    result.solved = true;
    result.approx_tts = exact_tts;
    result.bits_flipped = 0;

    for (uint32_t i = 0; i < m; ++i) {
        for (uint32_t x = 0; x < num_minterms; ++x) {
            if (d_vars[i][x].get(GRB_DoubleAttr_X) > 0.5) {
                if (kitty::get_bit(result.approx_tts[i], x)) {
                    kitty::clear_bit(result.approx_tts[i], x);
                } else {
                    kitty::set_bit(result.approx_tts[i], x);
                }
                ++result.bits_flipped;
            }
        }
    }

    return result;
}

TTApproxResult solve_ilp_ss_for_k(
    std::vector<kitty::dynamic_truth_table> const& exact_tts,
    CofactorANFData const& cof,
    TTApproxParams const& params,
    double time_limit_for_k) {

    uint32_t m = cof.m;
    uint32_t num_minterms = 1u << cof.n;
    uint32_t num_hd = cof.high_degree_indices.size();

    std::vector<double> weights = params.weights;
    if (weights.empty()) {
        weights.assign(num_minterms, 1.0 / num_minterms);
    }

    std::vector<std::vector<int32_t>> f_values(m, std::vector<int32_t>(num_minterms));
    for (uint32_t i = 0; i < m; ++i) {
        for (uint32_t x = 0; x < num_minterms; ++x) {
            f_values[i][x] = kitty::get_bit(exact_tts[i], x) ? 1 : 0;
        }
    }

    GRBEnv env(true);
    env.set(GRB_IntParam_OutputFlag, params.verbose ? 1 : 0);
    env.start();

    GRBModel model(env);
    if (time_limit_for_k > 0) {
        model.set(GRB_DoubleParam_TimeLimit, time_limit_for_k);
    }

    std::vector<std::vector<GRBVar>> d_vars(m);
    for (uint32_t i = 0; i < m; ++i) {
        d_vars[i].reserve(num_minterms);
        for (uint32_t x = 0; x < num_minterms; ++x) {
            double ub = 1.0;
            if (params.enable_pruning) {
                double bit_weight = bit_weight_for_output(i, m, params.register_bitsizes);
                if (weights[x] * bit_weight > params.error_bound) {
                    ub = 0.0;
                }
            }
            d_vars[i].push_back(model.addVar(0.0, ub, 0.0, GRB_BINARY,
                "d_" + std::to_string(i) + "_" + std::to_string(x)));
        }
    }

    std::vector<std::vector<GRBVar>> gamma_vars(m);
    std::vector<std::vector<GRBVar>> kappa_vars(m);
    for (uint32_t i = 0; i < m; ++i) {
        uint32_t cof_hd = cof.num_cofactors * num_hd;
        gamma_vars[i].reserve(cof_hd);
        kappa_vars[i].reserve(cof_hd);
        for (uint32_t idx = 0; idx < cof_hd; ++idx) {
            gamma_vars[i].push_back(model.addVar(0.0, 1.0, 0.0, GRB_BINARY,
                "g_" + std::to_string(i) + "_" + std::to_string(idx)));
            kappa_vars[i].push_back(model.addVar(0.0, GRB_INFINITY, 0.0, GRB_INTEGER,
                "kp_" + std::to_string(i) + "_" + std::to_string(idx)));
        }
    }

    std::vector<GRBVar> needed_vars;
    needed_vars.reserve(num_hd);
    for (uint32_t j = 0; j < num_hd; ++j) {
        needed_vars.push_back(model.addVar(0.0, 1.0, 0.0, GRB_BINARY,
            "nd_" + std::to_string(j)));
    }

    std::vector<GRBVar> alive_vars;
    alive_vars.reserve(num_hd);
    for (uint32_t j = 0; j < num_hd; ++j) {
        alive_vars.push_back(model.addVar(0.0, 1.0, 0.0, GRB_BINARY,
            "al_" + std::to_string(j)));
    }

    std::vector<GRBVar> e_vars;
    e_vars.reserve(num_minterms);
    for (uint32_t x = 0; x < num_minterms; ++x) {
        double ub = max_error_per_minterm(m, params.register_bitsizes);
        e_vars.push_back(model.addVar(0.0, ub, 0.0, GRB_CONTINUOUS,
            "e_" + std::to_string(x)));
    }

    for (uint32_t i = 0; i < m; ++i) {
        for (uint32_t c = 0; c < cof.num_cofactors; ++c) {
            for (uint32_t j = 0; j < num_hd; ++j) {
                uint32_t S = cof.high_degree_indices[j];
                uint32_t flat_idx = c * num_hd + j;

                GRBLinExpr sum_d = 0;
                uint32_t sub = S;
                do {
                    uint32_t global_x = c | (sub << cof.k);
                    sum_d += d_vars[i][global_x];
                    sub = (sub - 1) & S;
                } while (sub != S);

                model.addConstr(sum_d == 2 * kappa_vars[i][flat_idx] + gamma_vars[i][flat_idx]);
            }
        }
    }

    for (uint32_t j = 0; j < num_hd; ++j) {
        for (uint32_t i = 0; i < m; ++i) {
            for (uint32_t c = 0; c < cof.num_cofactors; ++c) {
                uint32_t flat_idx = c * num_hd + j;
                if (cof.original_coeff[i][flat_idx]) {
                    model.addConstr(needed_vars[j] >= 1 - gamma_vars[i][flat_idx]);
                } else {
                    model.addConstr(needed_vars[j] >= gamma_vars[i][flat_idx]);
                }
            }
        }
    }

    for (uint32_t j = 0; j < num_hd; ++j) {
        model.addConstr(alive_vars[j] >= needed_vars[j]);
        if (cof.parent_hd_idx[j] >= 0) {
            model.addConstr(alive_vars[cof.parent_hd_idx[j]] >= alive_vars[j]);
        }
    }

    if (params.register_bitsizes.empty()) {
        for (uint32_t x = 0; x < num_minterms; ++x) {
            GRBLinExpr delta = 0;
            for (uint32_t i = 0; i < m; ++i) {
                int32_t coeff = (1 - 2 * f_values[i][x]) * (1 << (m - 1 - i));
                delta += coeff * d_vars[i][x];
            }
            model.addConstr(e_vars[x] >= delta);
            model.addConstr(e_vars[x] >= -delta);
        }
    } else {
        uint32_t R = params.register_bitsizes.size();
        std::vector<std::vector<GRBVar>> er_vars(R);
        for (uint32_t r = 0; r < R; ++r) {
            er_vars[r].reserve(num_minterms);
            double ub = static_cast<double>((1u << params.register_bitsizes[r]) - 1);
            for (uint32_t x = 0; x < num_minterms; ++x) {
                er_vars[r].push_back(model.addVar(0.0, ub, 0.0, GRB_CONTINUOUS));
            }
        }

        uint32_t bit_offset = 0;
        for (uint32_t r = 0; r < R; ++r) {
            uint32_t bw = params.register_bitsizes[r];
            for (uint32_t x = 0; x < num_minterms; ++x) {
                GRBLinExpr delta_r = 0;
                for (uint32_t j = 0; j < bw; ++j) {
                    uint32_t i = bit_offset + j;
                    int32_t coeff = (1 - 2 * f_values[i][x]) * (1 << (bw - 1 - j));
                    delta_r += coeff * d_vars[i][x];
                }
                model.addConstr(er_vars[r][x] >= delta_r);
                model.addConstr(er_vars[r][x] >= -delta_r);
            }
            bit_offset += bw;
        }

        for (uint32_t x = 0; x < num_minterms; ++x) {
            GRBLinExpr sum_er = 0;
            for (uint32_t r = 0; r < R; ++r) {
                sum_er += er_vars[r][x];
            }
            model.addConstr(e_vars[x] >= sum_er);
        }
    }

    GRBLinExpr error_sum = 0;
    for (uint32_t x = 0; x < num_minterms; ++x) {
        error_sum += weights[x] * e_vars[x];
    }
    model.addConstr(error_sum <= params.error_bound);

    GRBLinExpr obj = 0;
    for (uint32_t j = 0; j < num_hd; ++j) {
        obj += alive_vars[j];
    }
    model.setObjective(obj, GRB_MINIMIZE);

    model.optimize();

    TTApproxResult result;
    result.best_k = static_cast<int>(cof.k);

    int sol_count = model.get(GRB_IntAttr_SolCount);
    if (sol_count == 0) {
        result.solved = false;
        result.approx_tts = exact_tts;
        result.ss_and_estimate = std::numeric_limits<uint32_t>::max();
        return result;
    }

    result.solved = true;
    result.approx_tts = exact_tts;
    result.bits_flipped = 0;

    for (uint32_t i = 0; i < m; ++i) {
        for (uint32_t x = 0; x < num_minterms; ++x) {
            if (d_vars[i][x].get(GRB_DoubleAttr_X) > 0.5) {
                if (kitty::get_bit(result.approx_tts[i], x)) {
                    kitty::clear_bit(result.approx_tts[i], x);
                } else {
                    kitty::set_bit(result.approx_tts[i], x);
                }
                ++result.bits_flipped;
            }
        }
    }

    uint32_t alive_count = 0;
    for (uint32_t j = 0; j < num_hd; ++j) {
        if (alive_vars[j].get(GRB_DoubleAttr_X) > 0.5) {
            ++alive_count;
        }
    }
    result.ss_and_estimate = alive_count + cof.k * m;

    return result;
}

#endif

} // namespace

uint32_t count_high_degree_monomials(
    std::vector<kitty::dynamic_truth_table> const& tts) {
    ANFData data = compute_anf_data(tts);
    return count_high_degree_active(data);
}

TTApproxResult approximate_truth_table_ilp(
    std::vector<kitty::dynamic_truth_table> const& exact_tts,
    TTApproxParams const& params) {

    assert(!exact_tts.empty());

    ANFData anf = compute_anf_data(exact_tts);
    uint32_t monomials_before = count_high_degree_active(anf);

#ifdef ENABLE_GUROBI
    TTApproxResult result;

    if (params.objective == TTApproxObjective::GlobalANF) {
        result = solve_ilp(exact_tts, anf, params);
    } else {
        uint32_t n = exact_tts[0].num_vars();

        if (params.fixed_k > 0) {
            CofactorANFData cof = compute_cofactor_anf_data(exact_tts, params.fixed_k);
            result = solve_ilp_ss_for_k(exact_tts, cof, params, params.time_limit);
        } else {
            uint32_t k_range = n - 1;
            double time_per_k = params.time_limit / std::max(k_range, 1u);

            TTApproxResult best;
            best.ss_and_estimate = std::numeric_limits<uint32_t>::max();
            best.solved = false;

            for (uint32_t k_val = 1; k_val < n; ++k_val) {
                CofactorANFData cof = compute_cofactor_anf_data(exact_tts, k_val);
                TTApproxResult candidate = solve_ilp_ss_for_k(exact_tts, cof, params, time_per_k);

                if (!candidate.solved) {
                    continue;
                }

                uint32_t total_cost = candidate.ss_and_estimate;
                if (total_cost < best.ss_and_estimate) {
                    best = std::move(candidate);
                }
            }

            if (best.solved) {
                result = std::move(best);
            } else {
                result.solved = false;
                result.approx_tts = exact_tts;
            }
        }

        if (result.solved) {
            result.ss_and_actual = lut_synth::count_ss_ands_for_k(result.approx_tts, result.best_k);
        }
    }

    result.monomials_before = monomials_before;

    if (result.solved) {
        result.monomials_after = count_high_degree_monomials(result.approx_tts);
        result.error = lut_synth::compute_integer_error(exact_tts, result.approx_tts, params.weights);
    } else {
        result.monomials_after = monomials_before;
        if (result.approx_tts.empty()) {
            result.approx_tts = exact_tts;
        }
    }

    return result;
#else
    (void)params;
    TTApproxResult result;
    result.approx_tts = exact_tts;
    result.monomials_before = monomials_before;
    result.monomials_after = monomials_before;
    result.solved = false;
    return result;
#endif
}

} // namespace lut_synth::approximate
