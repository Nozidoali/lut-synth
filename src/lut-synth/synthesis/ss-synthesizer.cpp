#include "lut-synth/synthesis/ss-synthesizer.hpp"
#include "lut-synth/synthesis/synthesis.hpp"
#include "lut-synth/truth-table.hpp"

namespace lut_synth {

SSSynthesizer::SSSynthesizer(int k, uint32_t num_random_starts,
                             uint64_t seed, bool disable_dont_care)
    : k_(k), num_random_starts_(num_random_starts), seed_(seed),
      disable_dont_care_(disable_dont_care) {}

std::string SSSynthesizer::name() const { return "ss"; }

mockturtle::xag_network
SSSynthesizer::synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const {
    return synthesize_selectswap(tts, k_, num_random_starts_, seed_);
}

mockturtle::xag_network SSSynthesizer::synthesize(TruthTable const &tt) const {
    if (!disable_dont_care_ && tt.has_dont_cares()) {
        return synthesize_selectswap_dontcare(tt.get_on_set(), tt.get_off_set(),
                                              k_, num_random_starts_, seed_);
    }
    return synthesize_selectswap(tt.get_tts(), k_, num_random_starts_, seed_);
}

} // namespace lut_synth
