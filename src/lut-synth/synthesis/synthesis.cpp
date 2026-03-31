#include "lut-synth/synthesis/synthesis.hpp"
#include "lut-synth/truth-table.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <string>

namespace lut_synth {

namespace {

std::string normalize_method(std::string_view method) {
    std::string lowered(method.begin(), method.end());
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowered;
}

} // namespace

mockturtle::xag_network synthesize_xag_network(std::vector<kitty::dynamic_truth_table> const &tts,
                                               std::string_view method, uint32_t ac_max_lut,
                                               int ss_k) {
    assert(!tts.empty());

    std::string normalized = normalize_method(method);
    if (normalized.empty()) {
        normalized = "ss";
    }

    SynthesisOptions opts;
    opts.ac_max_lut = ac_max_lut;
    opts.ss_k = ss_k;
    std::unique_ptr<XagSynthesizer> synth = XagSynthesizer::create(normalized, opts);
    assert(synth && "unknown synthesis method");
    return synth->synthesize(tts);
}

mockturtle::xag_network synthesize_xag_network(TruthTable const &tt, std::string_view method,
                                               uint32_t ac_max_lut, int ss_k,
                                               bool disable_dont_care) {
    assert(!tt.empty());

    std::string normalized = normalize_method(method);
    if (normalized.empty()) {
        normalized = "ss";
    }

    SynthesisOptions opts;
    opts.ac_max_lut = ac_max_lut;
    opts.ss_k = ss_k;
    opts.disable_dont_care = disable_dont_care;
    std::unique_ptr<XagSynthesizer> synth = XagSynthesizer::create(normalized, opts);
    assert(synth && "unknown synthesis method");
    return synth->synthesize(tt);
}

} // namespace lut_synth
