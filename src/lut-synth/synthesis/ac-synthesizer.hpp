#pragma once

#include "lut-synth/synthesis/xag-synthesizer.hpp"

namespace lut_synth {

/*! \brief XAG synthesis via affine classification. */
class ACSynthesizer : public XagSynthesizer {
  public:
    /*! \brief Construct with LUT size limit.
     *  \param max_lut_size Maximum LUT size for AC method
     */
    explicit ACSynthesizer(uint32_t max_lut_size = 4);

    std::string name() const override;
    mockturtle::xag_network
    synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const override;

  private:
    uint32_t max_lut_size_;
};

} // namespace lut_synth
