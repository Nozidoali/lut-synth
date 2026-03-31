#pragma once

#include <cstdint>
#include <memory>

#include <kitty/partial_truth_table.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/node_map.hpp>

#include "lut-synth/approximate/resynthesis/error-estimator.hpp"

namespace lut_synth::approximate {

/*! \brief VECBEE (Vector-based Boolean Difference Error Estimation).
 *
 *  Fast error estimation using Boolean difference computation.
 *  Estimates error by computing the Boolean difference between
 *  original and approximate outputs.
 */
class VECBEEEstimator : public ErrorEstimator {
public:
    using TT = kitty::partial_truth_table;

    /*! \brief Construct VECBEE estimator.
     *  \param num_patterns Number of simulation patterns
     *  \param seed Random seed
     *  \param metric Error metric type
     */
    VECBEEEstimator(uint32_t num_patterns, uint32_t seed, ErrorMetric metric);

    void initialize(Ntk const& ntk) override;
    double estimate(LAC const& lac) override;
    void update_after_apply(LAC const& lac) override;
    double accumulated_error() const override;
    ErrorMetric metric() const override;

    /*! \brief Get truth table for a node.
     *  \param n Node
     *  \return Truth table
     */
    TT const& get_tt(node n) const override;

    /*! \brief Get number of simulation bits.
     *  \return Number of bits
     */
    uint64_t num_bits() const override;

private:
    void compute_output_sensitivities();
    TT compute_boolean_difference(node n) const;
    double compute_error_from_bdiff(TT const& bdiff, TT const& error_mask) const;

    uint32_t num_patterns_;
    uint32_t seed_;
    ErrorMetric metric_;
    double accumulated_error_;
    uint64_t num_bits_;

    Ntk const* ntk_;
    std::unique_ptr<mockturtle::unordered_node_map<TT, Ntk>> tts_;
    std::unique_ptr<mockturtle::unordered_node_map<TT, Ntk>> output_sensitivities_;
};

} // namespace lut_synth::approximate
