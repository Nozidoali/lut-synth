#pragma once

#include "lut-synth/synthesis/xag-synthesizer.hpp"
#include "lut-synth/synthesis/rmdds.hpp"

namespace lut_synth {

/*! \brief XAG synthesis via Reed-Muller decision diagram method. */
class RMDDSSynthesizer : public XagSynthesizer {
  public:
    /*! \brief Construct with RMDDS parameters. */
    explicit RMDDSSynthesizer(RMDDSParams const &params = {});

    std::string name() const override;
    mockturtle::xag_network
    synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const override;

  private:
    RMDDSParams params_;
};

} // namespace lut_synth
