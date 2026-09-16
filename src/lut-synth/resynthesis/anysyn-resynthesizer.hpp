#pragma once

#include "lut-synth/resynthesis/xag-resynthesizer.hpp"
#include "lut-synth/resynthesis/resynthesis.hpp"

namespace lut_synth {

/*! \brief AnySyn resynthesis: multi-candidate downhill optimization. */
class AnySynResynthesizer : public XagResynthesizer {
  public:
    std::string name() const override;
    ResynthesisResult
    resynthesize_with_report(mockturtle::xag_network const &xag) const override;
    void set_timeout(double timeout_s) override;

  private:
    AnySynParams params_{};
};

} // namespace lut_synth
