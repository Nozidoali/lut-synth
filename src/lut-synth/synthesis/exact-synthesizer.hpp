#pragma once

#include "lut-synth/synthesis/xag-synthesizer.hpp"
#include "lut-synth/synthesis/synthesis.hpp"

namespace lut_synth {

/*! \brief XAG synthesis via SAT-based exact method. */
class ExactSynthesizer : public XagSynthesizer {
  public:
    /*! \brief Construct with exact synthesis parameters. */
    explicit ExactSynthesizer(ExactSynthesisParams const &params = {});

    std::string name() const override;
    mockturtle::xag_network
    synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const override;

  private:
    ExactSynthesisParams params_;
};

} // namespace lut_synth
