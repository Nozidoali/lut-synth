#include "lut-synth/synthesis/synthesis.hpp"

#include <kitty/bit_operations.hpp>
#include <kitty/operations.hpp>

#include <cassert>
#include <cstdint>
#include <string_view>
#include <vector>

namespace lut_synth::ss_detail {

kitty::dynamic_truth_table to_anf(kitty::dynamic_truth_table const &tt) {
    kitty::dynamic_truth_table anf = tt.construct();
    const int num_vars = static_cast<int>(tt.num_vars());

    std::vector<bool> values(1 << num_vars);
    for (int i = 0; i < (1 << num_vars); ++i) {
        values[i] = kitty::get_bit(tt, i);
    }

    for (int var = 0; var < num_vars; ++var) {
        for (int mask = 0; mask < (1 << num_vars); ++mask) {
            if (mask & (1 << var)) {
                values[mask] = values[mask] ^ values[mask ^ (1 << var)];
            }
        }
    }

    for (int i = 0; i < (1 << num_vars); ++i) {
        if (values[i]) {
            kitty::set_bit(anf, i);
        }
    }

    return anf;
}

mockturtle::xag_network::signal xor_tree(mockturtle::xag_network &network,
                                         std::vector<mockturtle::xag_network::signal> signals) {
    if (signals.empty()) {
        return network.get_constant(false);
    }
    while (signals.size() > 1) {
        std::vector<mockturtle::xag_network::signal> next;
        next.reserve((signals.size() + 1) / 2);
        for (size_t i = 0; i < signals.size(); i += 2) {
            next.push_back(i + 1 < signals.size() ? network.create_xor(signals[i], signals[i + 1])
                                                  : signals[i]);
        }
        signals.swap(next);
    }
    return signals.front();
}

mockturtle::xag_network::signal mux2(mockturtle::xag_network &network,
                                     mockturtle::xag_network::signal select,
                                     mockturtle::xag_network::signal a,
                                     mockturtle::xag_network::signal b) {
    return network.create_xor(a, network.create_and(select, network.create_xor(a, b)));
}

std::vector<mockturtle::xag_network::signal>
generate_product_terms(mockturtle::xag_network &network,
                       std::vector<mockturtle::xag_network::signal> const &pis) {
    std::vector<mockturtle::xag_network::signal> products;
    products.reserve(1u << pis.size());
    products.push_back(network.get_constant(true));
    for (auto const &pi : pis) {
        const size_t base = products.size();
        products.resize(base * 2);
        for (size_t i = 0; i < base; ++i) {
            products[i + base] = network.create_and(products[i], pi);
        }
    }
    return products;
}

std::vector<mockturtle::xag_network::signal>
select_outputs_from_anfs(mockturtle::xag_network &network,
                         std::vector<kitty::dynamic_truth_table> const &anfs,
                         std::vector<mockturtle::xag_network::signal> const &pis) {
    const std::vector<mockturtle::xag_network::signal> products = generate_product_terms(network, pis);
    std::vector<mockturtle::xag_network::signal> outputs;
    outputs.reserve(anfs.size());

    for (auto const &anf : anfs) {
        std::vector<mockturtle::xag_network::signal> terms;
        for (int i = 0; i < static_cast<int>(products.size()); ++i) {
            if (kitty::get_bit(anf, i)) {
                terms.push_back(products[i]);
            }
        }
        outputs.push_back(terms.empty() ? network.get_constant(false)
                                        : xor_tree(network, std::move(terms)));
    }

    return outputs;
}

std::vector<mockturtle::xag_network::signal> create_pis(mockturtle::xag_network &network,
                                                        int num_vars) {
    std::vector<mockturtle::xag_network::signal> pis;
    pis.reserve(num_vars);
    for (int i = 0; i < num_vars; ++i) {
        pis.push_back(network.create_pi());
    }
    return pis;
}

kitty::dynamic_truth_table permute_variables(kitty::dynamic_truth_table const &tt,
                                             std::vector<uint32_t> const &order) {
    kitty::dynamic_truth_table result(tt.num_vars());
    const uint64_t num_bits = tt.num_bits();
    const uint32_t num_vars = tt.num_vars();

    for (uint64_t i = 0; i < num_bits; ++i) {
        uint64_t new_index = 0;
        for (uint32_t v = 0; v < num_vars; ++v) {
            if ((i >> order[v]) & 1) {
                new_index |= (1ULL << v);
            }
        }
        if (kitty::get_bit(tt, i)) {
            kitty::set_bit(result, new_index);
        }
    }
    return result;
}

std::vector<kitty::dynamic_truth_table>
permute_all_variables(std::vector<kitty::dynamic_truth_table> const &tts,
                      std::vector<uint32_t> const &order) {
    std::vector<kitty::dynamic_truth_table> result;
    result.reserve(tts.size());
    for (auto const &tt : tts) {
        result.push_back(permute_variables(tt, order));
    }
    return result;
}

void build_mux_tree(mockturtle::xag_network &network,
                    std::vector<mockturtle::xag_network::signal> const &select_outputs,
                    std::vector<mockturtle::xag_network::signal> const &select_pis,
                    int num_outputs, int k) {
    const int cofactors = 1 << k;
    for (int output = 0; output < num_outputs; ++output) {
        std::vector<mockturtle::xag_network::signal> cof_outputs;
        cof_outputs.reserve(cofactors);
        for (int cof = 0; cof < cofactors; ++cof) {
            cof_outputs.push_back(select_outputs[cof * num_outputs + output]);
        }

        for (int level = 0; level < k; ++level) {
            const int step = 1 << level;
            for (int idx = 0; idx + step < static_cast<int>(cof_outputs.size()); idx += 2 * step) {
                cof_outputs[idx] = mux2(network, select_pis[level], cof_outputs[idx],
                                        cof_outputs[idx + step]);
            }
        }

        network.create_po(cof_outputs.front());
    }
}

} // namespace lut_synth::ss_detail

namespace lut_synth {

uint32_t check_tts(std::vector<kitty::dynamic_truth_table> const &tts,
                   [[maybe_unused]] std::string_view function_name) {
    assert(!tts.empty());
    const uint32_t num_vars = tts.front().num_vars();
    for ([[maybe_unused]] auto const &tt : tts) {
        assert(tt.num_vars() == num_vars);
    }
    return num_vars;
}

} // namespace lut_synth
