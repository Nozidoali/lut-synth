#include "lut-synth/resynthesis/v3-resynthesizer.hpp"

namespace lut_synth {

std::string V3Resynthesizer::name() const { return "v3"; }

void V3Resynthesizer::set_timeout(double timeout_s) { params_.timeout_s = timeout_s; }

ResynthesisResult
V3Resynthesizer::resynthesize_with_report(mockturtle::xag_network const &xag) const {
    return resynthesize_xag_v3_with_report(xag, params_);
}

} // namespace lut_synth
