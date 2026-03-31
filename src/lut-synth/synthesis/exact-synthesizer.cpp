#include "lut-synth/synthesis/exact-synthesizer.hpp"

namespace lut_synth {

ExactSynthesizer::ExactSynthesizer(ExactSynthesisParams const &params)
    : params_(params) {}

std::string ExactSynthesizer::name() const { return "exact"; }

mockturtle::xag_network
ExactSynthesizer::synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const {
    return synthesize_exact(tts, params_);
}

} // namespace lut_synth
