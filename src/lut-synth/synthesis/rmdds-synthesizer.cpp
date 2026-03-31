#include "lut-synth/synthesis/rmdds-synthesizer.hpp"

namespace lut_synth {

RMDDSSynthesizer::RMDDSSynthesizer(RMDDSParams const &params)
    : params_(params) {}

std::string RMDDSSynthesizer::name() const { return "rmdds"; }

mockturtle::xag_network
RMDDSSynthesizer::synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const {
    return synthesize_rmdds(tts, params_);
}

} // namespace lut_synth
