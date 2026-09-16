#include "lut-synth/resynthesis/anysyn-resynthesizer.hpp"

namespace lut_synth {

std::string AnySynResynthesizer::name() const { return "anysyn"; }

void AnySynResynthesizer::set_timeout(double timeout_s) { params_.timeout_s = timeout_s; }

ResynthesisResult
AnySynResynthesizer::resynthesize_with_report(mockturtle::xag_network const &xag) const {
    return resynthesize_xag_anysyn_with_report(xag, params_);
}

} // namespace lut_synth
