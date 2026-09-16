#include "lut-synth/synthesis/xag-synthesizer.hpp"
#include "lut-synth/synthesis/davio-synthesizer.hpp"
#include "lut-synth/synthesis/dsd-synthesizer.hpp"
#include "lut-synth/synthesis/exact-synthesizer.hpp"
#include "lut-synth/synthesis/ss-synthesizer.hpp"
#include "lut-synth/truth-table.hpp"

#include <cassert>
#include <unordered_map>

namespace lut_synth {

mockturtle::xag_network
synthesize_positive_davio(std::vector<kitty::dynamic_truth_table> const &tts);
mockturtle::xag_network synthesize_dsd(std::vector<kitty::dynamic_truth_table> const &tts);

std::string DavioSynthesizer::name() const { return "davio"; }

mockturtle::xag_network
DavioSynthesizer::synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const {
    return synthesize_positive_davio(tts);
}

std::string DSDSynthesizer::name() const { return "dsd"; }

mockturtle::xag_network
DSDSynthesizer::synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const {
    return synthesize_dsd(tts);
}

ExactSynthesizer::ExactSynthesizer(ExactSynthesisParams const &params)
    : params_(params) {}

std::string ExactSynthesizer::name() const { return "exact"; }

mockturtle::xag_network
ExactSynthesizer::synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const {
    return synthesize_exact(tts, params_);
}

namespace {

using Registry = std::unordered_map<std::string, XagSynthesizer::ParameterizedFactory>;

Registry &registry() {
    static Registry r;
    return r;
}

void ensure_registered() {
    static bool done = false;
    if (done) return;
    done = true;

    auto reg = [](std::string const &name, XagSynthesizer::ParameterizedFactory f) {
        registry()[name] = std::move(f);
    };

    reg("davio", [](SynthesisOptions const &) { return std::make_unique<DavioSynthesizer>(); });
    reg("positive_davio", [](SynthesisOptions const &) { return std::make_unique<DavioSynthesizer>(); });
    reg("dsd", [](SynthesisOptions const &) { return std::make_unique<DSDSynthesizer>(); });
    reg("ss", [](SynthesisOptions const &opts) {
        return std::make_unique<SSSynthesizer>(opts.ss_k, opts.num_random_starts,
                                                opts.seed, opts.disable_dont_care);
    });
    reg("exact", [](SynthesisOptions const &) { return std::make_unique<ExactSynthesizer>(); });
    reg("exact_mc", [](SynthesisOptions const &) { return std::make_unique<ExactSynthesizer>(); });
}

} // namespace

mockturtle::xag_network XagSynthesizer::synthesize(TruthTable const &tt) const {
    return synthesize(tt.get_tts());
}

void XagSynthesizer::register_method(std::string const &name, Factory factory) {
    registry()[name] = [f = std::move(factory)](SynthesisOptions const &) {
        return f();
    };
}

void XagSynthesizer::register_parameterized(std::string const &name, ParameterizedFactory factory) {
    registry()[name] = std::move(factory);
}

std::unique_ptr<XagSynthesizer> XagSynthesizer::create(std::string const &name) {
    return create(name, SynthesisOptions{});
}

std::unique_ptr<XagSynthesizer> XagSynthesizer::create(std::string const &name,
                                                        SynthesisOptions const &opts) {
    ensure_registered();
    Registry &r = registry();
    auto it = r.find(name);
    if (it == r.end()) {
        return nullptr;
    }
    return it->second(opts);
}

std::vector<std::string> XagSynthesizer::registered_methods() {
    ensure_registered();
    std::vector<std::string> names;
    for (auto const &[k, v] : registry()) {
        names.push_back(k);
    }
    return names;
}

} // namespace lut_synth
