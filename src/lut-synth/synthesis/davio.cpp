#include "lut-synth/synthesis/synthesis.hpp"

#include <kitty/operations.hpp>
#include <mockturtle/algorithms/decomposition.hpp>

#include <numeric>

namespace lut_synth {

mockturtle::xag_network
synthesize_positive_davio(std::vector<kitty::dynamic_truth_table> const &tts) {
    const uint32_t num_vars = check_tts(tts, "synthesize_positive_davio");

    mockturtle::xag_network network;

    if (num_vars == 0u) {
        for (auto const &tt : tts) {
            network.create_po(kitty::get_bit(tt, 0u) ? network.get_constant(true)
                                                     : network.get_constant(false));
        }
        return network;
    }

    std::vector<mockturtle::xag_network::signal> inputs(num_vars);
    std::generate(inputs.begin(), inputs.end(), [&]() { return network.create_pi(); });

    std::vector<uint32_t> variable_order(num_vars);
    std::iota(variable_order.begin(), variable_order.end(), 0u);

    for (auto const &tt : tts) {
        network.create_po(
            mockturtle::positive_davio_decomposition(network, tt, variable_order, inputs));
    }

    return network;
}

} // namespace lut_synth
