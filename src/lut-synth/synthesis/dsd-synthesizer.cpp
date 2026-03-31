#include "lut-synth/synthesis/dsd-synthesizer.hpp"

namespace lut_synth {

mockturtle::xag_network synthesize_dsd(std::vector<kitty::dynamic_truth_table> const &tts);

std::string DSDSynthesizer::name() const { return "dsd"; }

mockturtle::xag_network
DSDSynthesizer::synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const {
    return synthesize_dsd(tts);
}

} // namespace lut_synth
