#pragma once

#include "lut-synth/synthesis/xag-synthesizer.hpp"

namespace lut_synth {

/*! \brief XAG synthesis via positive Davio decomposition. */
class DavioSynthesizer : public XagSynthesizer {
  public:
    std::string name() const override;
    mockturtle::xag_network
    synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const override;
};

} // namespace lut_synth
