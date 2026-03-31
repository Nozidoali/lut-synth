#include "lut-synth/synthesis/anf-xag-builder.hpp"
#include "lut-synth/synthesis/synthesis.hpp"

#include <kitty/operations.hpp>

namespace lut_synth {

namespace {

uint32_t hamming_distance(uint32_t a, uint32_t b) {
    return __builtin_popcount(a ^ b);
}

} // namespace

std::vector<uint32_t> extract_anf_terms(kitty::dynamic_truth_table const &anf) {
    std::vector<uint32_t> terms;
    uint64_t num_bits = 1ull << anf.num_vars();
    for (uint64_t i = 0; i < num_bits; ++i) {
        if (kitty::get_bit(anf, i)) {
            terms.push_back(static_cast<uint32_t>(i));
        }
    }
    return terms;
}

std::vector<uint32_t> reorder_terms_by_hamming(std::vector<uint32_t> const &terms) {
    if (terms.size() <= 1) {
        return terms;
    }

    std::vector<uint32_t> ordered;
    ordered.reserve(terms.size());
    std::vector<bool> used(terms.size(), false);

    size_t min_weight_idx = 0;
    uint32_t min_weight = __builtin_popcount(terms[0]);
    for (size_t i = 1; i < terms.size(); ++i) {
        uint32_t w = __builtin_popcount(terms[i]);
        if (w < min_weight) {
            min_weight = w;
            min_weight_idx = i;
        }
    }

    ordered.push_back(terms[min_weight_idx]);
    used[min_weight_idx] = true;

    while (ordered.size() < terms.size()) {
        uint32_t current = ordered.back();
        size_t best_idx = 0;
        uint32_t best_dist = UINT32_MAX;
        for (size_t i = 0; i < terms.size(); ++i) {
            if (!used[i]) {
                uint32_t dist = hamming_distance(current, terms[i]);
                if (dist < best_dist) {
                    best_dist = dist;
                    best_idx = i;
                }
            }
        }
        ordered.push_back(terms[best_idx]);
        used[best_idx] = true;
    }

    return ordered;
}

mockturtle::xag_network::signal build_product_cached(
    mockturtle::xag_network &network,
    std::vector<mockturtle::xag_network::signal> const &inputs,
    uint32_t term,
    std::unordered_map<uint32_t, mockturtle::xag_network::signal> &cache) {
    using signal = mockturtle::xag_network::signal;

    if (term == 0) {
        return network.get_constant(true);
    }

    std::unordered_map<uint32_t, signal>::iterator it = cache.find(term);
    if (it != cache.end()) {
        return it->second;
    }

    uint32_t best_subset = 0;
    for (auto const &[cached_term, _] : cache) {
        if ((cached_term & term) == cached_term) {
            if (__builtin_popcount(cached_term) > __builtin_popcount(best_subset)) {
                best_subset = cached_term;
            }
        }
    }

    signal result;
    if (best_subset != 0) {
        result = cache[best_subset];
        uint32_t remaining = term ^ best_subset;
        for (uint32_t v = 0; v < inputs.size(); ++v) {
            if (remaining & (1u << v)) {
                result = network.create_and(result, inputs[v]);
            }
        }
    } else {
        result = network.get_constant(true);
        for (uint32_t v = 0; v < inputs.size(); ++v) {
            if (term & (1u << v)) {
                result = network.create_and(result, inputs[v]);
            }
        }
    }

    cache[term] = result;
    return result;
}

mockturtle::xag_network::signal build_xag_from_terms(
    mockturtle::xag_network &network,
    std::vector<mockturtle::xag_network::signal> const &inputs,
    std::vector<uint32_t> const &terms,
    std::unordered_map<uint32_t, mockturtle::xag_network::signal> &cache) {
    using signal = mockturtle::xag_network::signal;

    if (terms.empty()) {
        return network.get_constant(false);
    }

    std::vector<signal> products;
    products.reserve(terms.size());
    for (uint32_t term : terms) {
        products.push_back(build_product_cached(network, inputs, term, cache));
    }

    return ss_detail::xor_tree(network, std::move(products));
}

} // namespace lut_synth
