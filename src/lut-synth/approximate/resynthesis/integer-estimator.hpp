#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <kitty/partial_truth_table.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/node_map.hpp>

#include "lut-synth/approximate/resynthesis/error-estimator.hpp"

namespace lut_synth::approximate {

/*! \brief Integer-aware error estimator for ResubALS.
 *
 *  Treats multi-bit outputs as unsigned integers (MSB first). Error is
 *  measured as weighted mean absolute integer deviation rather than per-bit.
 *  This allows aggressive approximation of low-order output bits while
 *  preserving high-order bits.
 */
class IntegerEstimator : public ErrorEstimator {
public:
    using TT = kitty::partial_truth_table;

    /*! \brief Construct integer-aware estimator.
     *  \param num_patterns Number of simulation patterns
     *  \param seed Random seed
     *  \param weights Per-input weights (empty = uniform)
     */
    IntegerEstimator(uint32_t num_patterns, uint32_t seed,
                     std::vector<double> const& weights = {},
                     uint32_t exhaustive_threshold = 12);

    void initialize(Ntk const& ntk) override;
    double estimate(LAC const& lac) override;
    uint64_t estimate_count(LAC const& lac) override;
    /*! \brief Max absolute integer deviation over simulation patterns for this LAC. */
    uint32_t estimate_max_per_pattern(LAC const& lac);
    void update_after_apply(LAC const& lac) override;
    double accumulated_error() const override;
    uint64_t accumulated_error_count() const override;
    ErrorMetric metric() const override;
    TT const& get_tt(node n) const override;
    uint64_t num_bits() const override;

private:
    TT compute_candidate_tt(LAC const& lac) const;
    double compute_integer_error_for_lac(LAC const& lac) const;
    uint64_t compute_integer_error_count_for_lac(LAC const& lac) const;
    uint32_t compute_max_integer_error_for_lac(LAC const& lac) const;

    std::vector<uint32_t> collect_affected_outputs(LAC const& lac) const;
    uint32_t new_value_at(uint64_t x, TT const& candidate,
                          std::vector<bool> const& is_affected_po) const;

    template <typename Visit>
    void for_each_changed_pattern(LAC const& lac, Visit visit) const;

    uint32_t num_patterns_;
    uint32_t seed_;
    uint32_t exhaustive_threshold_;
    std::vector<double> weights_;
    double accumulated_error_;
    uint64_t accumulated_error_count_;
    uint64_t num_bits_;

    Ntk const* ntk_;
    std::unique_ptr<mockturtle::unordered_node_map<TT, Ntk>> tts_;
    std::vector<uint32_t> exact_integers_;
    std::vector<uint32_t> po_nodes_;
    std::vector<bool> po_complemented_;
};

} // namespace lut_synth::approximate
