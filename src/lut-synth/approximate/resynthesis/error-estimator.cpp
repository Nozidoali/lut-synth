#include "lut-synth/approximate/resynthesis/error-estimator.hpp"

#include <mockturtle/algorithms/simulation.hpp>

namespace lut_synth::approximate {

uint64_t ErrorEstimator::estimate_count(LAC const& lac) {
    double err = estimate(lac);
    return static_cast<uint64_t>(err * num_bits());
}

uint64_t ErrorEstimator::accumulated_error_count() const {
    return static_cast<uint64_t>(accumulated_error() * num_bits());
}

double ErrorEstimator::verify_error(Ntk const& original, Ntk const& approximate) {
    uint64_t count = verify_error_count(original, approximate);
    uint64_t bits = num_bits();
    if (bits == 0) return 0.0;
    return static_cast<double>(count) / bits;
}

uint64_t ErrorEstimator::verify_error_count(Ntk const& original,
                                             Ntk const& approximate) {
    if (original.num_pos() != approximate.num_pos()) return num_bits();
    if (original.num_pos() == 0) return 0;

    std::mt19937 rng(42);
    uint32_t np = static_cast<uint32_t>(num_bits());
    mockturtle::partial_simulator sim_orig(original.num_pis(), np, rng());
    mockturtle::partial_simulator sim_approx(approximate.num_pis(), np, rng());

    mockturtle::unordered_node_map<TT, Ntk> tts_orig(original);
    mockturtle::unordered_node_map<TT, Ntk> tts_approx(approximate);

    mockturtle::simulate_nodes(original, tts_orig, sim_orig);
    mockturtle::simulate_nodes(approximate, tts_approx, sim_approx);

    uint64_t error_count = 0;

    switch (metric()) {
    case ErrorMetric::ER: {
        TT any_diff(np);
        original.foreach_po([&](auto const& f, auto i) {
            node orig_node = original.get_node(f);
            TT orig_tt = tts_orig[orig_node];
            if (original.is_complemented(f)) {
                for (auto& block : orig_tt._bits) block = ~block;
            }

            approximate.foreach_po([&](auto const& g, auto j) {
                if (i != j) return;
                node approx_node = approximate.get_node(g);
                TT approx_tt = tts_approx[approx_node];
                if (approximate.is_complemented(g)) {
                    for (auto& block : approx_tt._bits) block = ~block;
                }

                for (size_t k = 0; k < orig_tt._bits.size(); ++k) {
                    any_diff._bits[k] |= (orig_tt._bits[k] ^ approx_tt._bits[k]);
                }
            });
        });

        for (size_t k = 0; k < any_diff._bits.size(); ++k) {
            error_count += __builtin_popcountll(any_diff._bits[k]);
        }
        break;
    }
    case ErrorMetric::MHD:
    case ErrorMetric::MSE: {
        original.foreach_po([&](auto const& f, auto i) {
            node orig_node = original.get_node(f);
            TT orig_tt = tts_orig[orig_node];
            if (original.is_complemented(f)) {
                for (auto& block : orig_tt._bits) block = ~block;
            }

            approximate.foreach_po([&](auto const& g, auto j) {
                if (i != j) return;
                node approx_node = approximate.get_node(g);
                TT approx_tt = tts_approx[approx_node];
                if (approximate.is_complemented(g)) {
                    for (auto& block : approx_tt._bits) block = ~block;
                }

                for (size_t k = 0; k < orig_tt._bits.size(); ++k) {
                    error_count += __builtin_popcountll(
                        orig_tt._bits[k] ^ approx_tt._bits[k]);
                }
            });
        });
        break;
    }
    default:
        break;
    }

    return error_count;
}

} // namespace lut_synth::approximate
