#include "lut-synth/resynthesis/xag-resynthesizer.hpp"
#include "lut-synth/resynthesis/resynthesis.hpp"
#include "lut-synth/resynthesis/anysyn-resynthesizer.hpp"

#include <unordered_map>

namespace lut_synth {

namespace {

using Registry = std::unordered_map<std::string, XagResynthesizer::Factory>;

Registry &registry() {
    static Registry r;
    return r;
}

void ensure_registered() {
    static bool done = false;
    if (done) return;
    done = true;

    auto reg = [](std::string const &name, XagResynthesizer::Factory f) {
        registry()[name] = std::move(f);
    };

    reg("anysyn", [] { return std::make_unique<AnySynResynthesizer>(); });
    reg("", [] { return std::make_unique<AnySynResynthesizer>(); });
}

} // namespace

mockturtle::xag_network
XagResynthesizer::resynthesize(mockturtle::xag_network const &xag) const {
    return resynthesize_with_report(xag).xag;
}

void XagResynthesizer::register_method(std::string const &name, Factory factory) {
    registry()[name] = std::move(factory);
}

std::unique_ptr<XagResynthesizer> XagResynthesizer::create(std::string const &name) {
    ensure_registered();
    Registry &r = registry();
    Registry::iterator it = r.find(name);
    if (it == r.end()) return nullptr;
    return it->second();
}

std::vector<std::string> XagResynthesizer::registered_methods() {
    ensure_registered();
    std::vector<std::string> names;
    for (auto const &[k, v] : registry()) {
        names.push_back(k);
    }
    return names;
}

} // namespace lut_synth
