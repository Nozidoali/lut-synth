#include "lut-synth/approximate/tt-approximation/bounds.hpp"
#include "lut-synth/synthesis/synthesis.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <set>
#include <vector>

#include <kitty/bit_operations.hpp>
#include <kitty/dynamic_truth_table.hpp>

namespace lut_synth::approximate {

namespace {

std::vector<std::vector<bool>> compute_anf_coefficients(
    std::vector<kitty::dynamic_truth_table> const& tts) {
    uint32_t m = tts.size();
    uint32_t num_vars = tts[0].num_vars();
    uint32_t num_minterms = 1u << num_vars;

    std::vector<std::vector<bool>> coeffs(m);
    for (uint32_t i = 0; i < m; ++i) {
        kitty::dynamic_truth_table anf = ss_detail::to_anf(tts[i]);
        coeffs[i].resize(num_minterms);
        for (uint32_t s = 0; s < num_minterms; ++s) {
            coeffs[i][s] = kitty::get_bit(anf, s);
        }
    }
    return coeffs;
}

std::vector<std::vector<bool>> compute_cofactor_anfs(
    std::vector<kitty::dynamic_truth_table> const& tts, uint32_t k) {
    uint32_t n = tts[0].num_vars();
    uint32_t m = tts.size();
    uint32_t n_rem = n - k;
    uint32_t num_cofactors = 1u << k;
    uint32_t num_anf_minterms = 1u << n_rem;

    std::vector<std::vector<bool>> all_coeffs;
    all_coeffs.reserve(num_cofactors * m);

    for (uint32_t c = 0; c < num_cofactors; ++c) {
        for (uint32_t i = 0; i < m; ++i) {
            kitty::dynamic_truth_table cof_tt(n_rem);
            for (uint32_t t = 0; t < num_anf_minterms; ++t) {
                uint32_t global_idx = c | (t << k);
                if (kitty::get_bit(tts[i], global_idx)) {
                    kitty::set_bit(cof_tt, t);
                }
            }
            kitty::dynamic_truth_table anf = ss_detail::to_anf(cof_tt);
            std::vector<bool> coeffs(num_anf_minterms);
            for (uint32_t s = 0; s < num_anf_minterms; ++s) {
                coeffs[s] = kitty::get_bit(anf, s);
            }
            all_coeffs.push_back(std::move(coeffs));
        }
    }
    return all_coeffs;
}

std::vector<std::vector<bool>> compute_cofactor_tts(
    std::vector<kitty::dynamic_truth_table> const& tts, uint32_t k) {
    uint32_t n = tts[0].num_vars();
    uint32_t m = tts.size();
    uint32_t n_rem = n - k;
    uint32_t num_cofactors = 1u << k;
    uint32_t num_entries = 1u << n_rem;

    std::vector<std::vector<bool>> all_tts;
    all_tts.reserve(num_cofactors * m);

    for (uint32_t c = 0; c < num_cofactors; ++c) {
        for (uint32_t i = 0; i < m; ++i) {
            std::vector<bool> tt(num_entries);
            for (uint32_t t = 0; t < num_entries; ++t) {
                uint32_t global_idx = c | (t << k);
                tt[t] = kitty::get_bit(tts[i], global_idx);
            }
            all_tts.push_back(std::move(tt));
        }
    }
    return all_tts;
}

uint32_t compute_mux_and_cost(
    std::vector<std::vector<bool>> const& cof_tts,
    uint32_t k, uint32_t m, uint32_t n_rem) {
    uint32_t num_cofactors = 1u << k;
    uint32_t total_ands = 0;

    for (uint32_t level = 0; level < k; ++level) {
        uint32_t step = 1u << level;
        uint32_t diff_num_vars = level + n_rem;
        uint32_t diff_size = 1u << diff_num_vars;
        uint32_t swap_mask = (1u << level) - 1;

        std::set<std::vector<bool>> distinct_diffs;

        for (uint32_t o = 0; o < m; ++o) {
            for (uint32_t i = 0; i + step < num_cofactors;
                 i += 2 * step) {
                std::vector<bool> diff_tt(diff_size);
                bool all_same = true;

                for (uint32_t a = 0; a < diff_size; ++a) {
                    uint32_t swap_offset = a & swap_mask;
                    uint32_t rem_idx = a >> level;
                    uint32_t left_cof = i + swap_offset;
                    uint32_t right_cof = i + step + swap_offset;

                    bool left = cof_tts[left_cof * m + o][rem_idx];
                    bool right = cof_tts[right_cof * m + o][rem_idx];
                    diff_tt[a] = left != right;

                    if (a > 0 && diff_tt[a] != diff_tt[0])
                        all_same = false;
                }

                bool is_constant = all_same;
                if (!is_constant) {
                    distinct_diffs.insert(diff_tt);
                }
            }
        }

        total_ands += distinct_diffs.size();
    }

    return total_ands;
}

} // namespace

uint32_t count_alive_monomials(
    std::vector<std::vector<bool>> const& anf_coefficients,
    uint32_t num_vars) {
    uint32_t num_minterms = 1u << num_vars;
    std::vector<bool> alive(num_minterms, false);

    for (uint32_t s = 0; s < num_minterms; ++s) {
        if (__builtin_popcount(s) < 2) continue;
        for (auto const& coeffs : anf_coefficients) {
            if (coeffs[s]) {
                alive[s] = true;
                break;
            }
        }
    }

    for (int deg = static_cast<int>(num_vars); deg >= 3; --deg) {
        for (uint32_t s = 0; s < num_minterms; ++s) {
            if (__builtin_popcount(s) != deg) continue;
            if (!alive[s]) continue;
            uint32_t msb = 31 - __builtin_clz(s);
            uint32_t parent = s ^ (1u << msb);
            if (__builtin_popcount(parent) >= 2) {
                alive[parent] = true;
            }
        }
    }

    uint32_t count = 0;
    for (uint32_t s = 0; s < num_minterms; ++s) {
        if (__builtin_popcount(s) >= 2 && alive[s]) {
            ++count;
        }
    }
    return count;
}

AnalyticalBounds compute_analytical_bounds(
    std::vector<kitty::dynamic_truth_table> const& tts) {
    assert(!tts.empty());
    uint32_t n = tts[0].num_vars();
    uint32_t m = tts.size();
    assert(n >= 2);

    AnalyticalBounds bounds;

    std::vector<std::vector<bool>> direct_coeffs = compute_anf_coefficients(tts);
    uint32_t select_alive = count_alive_monomials(direct_coeffs, n);
    bounds.select = {0, select_alive, 0, select_alive};
    bounds.all_k.push_back(bounds.select);

    bounds.best_select_swap = {-1, 0, 0, std::numeric_limits<uint32_t>::max()};

    for (uint32_t k = 1; k < n; ++k) {
        uint32_t n_rem = n - k;
        std::vector<std::vector<bool>> cof_anfs = compute_cofactor_anfs(tts, k);
        std::vector<std::vector<bool>> cof_tts = compute_cofactor_tts(tts, k);
        uint32_t alive = (n_rem >= 2)
            ? count_alive_monomials(cof_anfs, n_rem) : 0;
        uint32_t swap = compute_mux_and_cost(cof_tts, k, m, n_rem);
        uint32_t total = alive + swap;

        BoundEntry entry{static_cast<int>(k), alive, swap, total};
        bounds.all_k.push_back(entry);

        if (total < bounds.best_select_swap.total) {
            bounds.best_select_swap = entry;
        }
    }

    if (bounds.best_select_swap.k == -1) {
        bounds.best_select_swap = bounds.select;
    }

    return bounds;
}

} // namespace lut_synth::approximate
