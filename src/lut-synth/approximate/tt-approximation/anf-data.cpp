#include "lut-synth/approximate/tt-approximation/anf-data.hpp"
#include "lut-synth/synthesis/synthesis.hpp"

#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <kitty/bit_operations.hpp>
#include <kitty/dynamic_truth_table.hpp>

namespace lut_synth::approximate {

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

} // namespace lut_synth::approximate
