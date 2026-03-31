#include "lut-synth/synthesis/xag-synthesizer.hpp"
#include "lut-synth/synthesis/ac-synthesizer.hpp"
#include "lut-synth/synthesis/davio-synthesizer.hpp"
#include "lut-synth/synthesis/dsd-synthesizer.hpp"
#include "lut-synth/synthesis/exact-synthesizer.hpp"
#include "lut-synth/synthesis/rmdds-synthesizer.hpp"
#include "lut-synth/synthesis/ss-synthesizer.hpp"
#include "lut-synth/truth-table.hpp"

#include <unordered_map>

namespace lut_synth {

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
    reg("ac", [](SynthesisOptions const &opts) {
        return std::make_unique<ACSynthesizer>(opts.ac_max_lut);
    });
    reg("dsd", [](SynthesisOptions const &) { return std::make_unique<DSDSynthesizer>(); });
    reg("ss", [](SynthesisOptions const &opts) {
        return std::make_unique<SSSynthesizer>(opts.ss_k, opts.num_random_starts,
                                                opts.seed, opts.disable_dont_care);
    });
    reg("exact", [](SynthesisOptions const &) { return std::make_unique<ExactSynthesizer>(); });
    reg("exact_mc", [](SynthesisOptions const &) { return std::make_unique<ExactSynthesizer>(); });
    reg("rmdds", [](SynthesisOptions const &) { return std::make_unique<RMDDSSynthesizer>(); });
    reg("rm", [](SynthesisOptions const &) { return std::make_unique<RMDDSSynthesizer>(); });
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
