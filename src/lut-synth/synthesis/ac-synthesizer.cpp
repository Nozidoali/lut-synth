#include "lut-synth/synthesis/ac-synthesizer.hpp"

#include <cassert>

namespace lut_synth {

mockturtle::xag_network synthesize_ac(std::vector<kitty::dynamic_truth_table> const &tts,
                                      uint32_t max_lut_size);

ACSynthesizer::ACSynthesizer(uint32_t max_lut_size) : max_lut_size_(max_lut_size) {}

std::string ACSynthesizer::name() const { return "ac"; }

mockturtle::xag_network
ACSynthesizer::synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const {
    assert(max_lut_size_ > 0u);
    return synthesize_ac(tts, max_lut_size_);
}

} // namespace lut_synth
