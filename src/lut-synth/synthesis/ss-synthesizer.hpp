#pragma once

#include "lut-synth/synthesis/xag-synthesizer.hpp"

namespace lut_synth {

/*! \brief XAG synthesis via select-swap (SS) method.
 *
 *  Supports don't-care exploitation when synthesizing from TruthTable.
 */
class SSSynthesizer : public XagSynthesizer {
  public:
    /*! \brief Construct with SS-specific parameters.
     *  \param k Shannon parameter (-1 for auto)
     *  \param num_random_starts Number of random variable orderings to try
     *  \param seed Random seed for multi-start
     *  \param disable_dont_care If true, ignore don't-cares
     */
    explicit SSSynthesizer(int k = -1, uint32_t num_random_starts = 1,
                           uint64_t seed = 0, bool disable_dont_care = false);

    std::string name() const override;
    mockturtle::xag_network
    synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const override;
    mockturtle::xag_network synthesize(lut_synth::TruthTable const &tt) const override;
    bool supports_dont_care() const override { return !disable_dont_care_; }

  private:
    int k_;
    uint32_t num_random_starts_;
    uint64_t seed_;
    bool disable_dont_care_;
};

} // namespace lut_synth
