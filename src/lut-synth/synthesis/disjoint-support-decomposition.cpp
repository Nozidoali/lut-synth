#include "lut-synth/synthesis/synthesis.hpp"

#include <mockturtle/algorithms/decomposition.hpp>
#include <mockturtle/algorithms/dsd_decomposition.hpp>

#include <numeric>

namespace lut_synth {

mockturtle::xag_network synthesize_dsd(std::vector<kitty::dynamic_truth_table> const &tts) {
    const uint32_t num_vars = check_tts(tts, "synthesize_dsd");

    mockturtle::xag_network network;
    std::vector<mockturtle::xag_network::signal> pis(num_vars);
    std::generate(pis.begin(), pis.end(), [&]() { return network.create_pi(); });

    auto on_prime = [&](kitty::dynamic_truth_table const &remainder,
                        std::vector<mockturtle::xag_network::signal> const &child_signals) {
        std::vector<uint32_t> vars(remainder.num_vars());
        std::iota(vars.begin(), vars.end(), 0u);
        return mockturtle::shannon_decomposition(network, remainder, vars, child_signals);
    };

    for (auto const &tt : tts) {
        network.create_po(mockturtle::dsd_decomposition(network, tt, pis, on_prime));
    }

    return network;
}

} // namespace lut_synth
