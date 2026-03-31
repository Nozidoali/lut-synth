#include "lut-synth/synthesis/davio-synthesizer.hpp"

namespace lut_synth {

mockturtle::xag_network
synthesize_positive_davio(std::vector<kitty::dynamic_truth_table> const &tts);

std::string DavioSynthesizer::name() const { return "davio"; }

mockturtle::xag_network
DavioSynthesizer::synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const {
    return synthesize_positive_davio(tts);
}

} // namespace lut_synth
