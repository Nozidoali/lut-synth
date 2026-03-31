#pragma once

#include "lut-synth/resynthesis/xag-resynthesizer.hpp"
#include "lut-synth/resynthesis/resynthesis.hpp"

namespace lut_synth {

/*! \brief V3 resynthesis: multi-candidate downhill optimization. */
class V3Resynthesizer : public XagResynthesizer {
  public:
    std::string name() const override;
    ResynthesisResult
    resynthesize_with_report(mockturtle::xag_network const &xag) const override;
    void set_timeout(double timeout_s) override;

  private:
    ResynthesisV3Params params_{};
};

} // namespace lut_synth
