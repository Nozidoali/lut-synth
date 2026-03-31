#pragma once

#include <cstdint>
#include <memory>

#include <kitty/partial_truth_table.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/node_map.hpp>

#include "lut-synth/approximate/resynthesis/error-estimator.hpp"

namespace lut_synth::approximate {

/*! \brief Simulation-based error estimator using random patterns. */
class SimulationEstimator : public ErrorEstimator {
public:
    using TT = kitty::partial_truth_table;

    /*! \brief Construct simulation estimator.
     *  \param num_patterns Number of simulation patterns
     *  \param seed Random seed
     *  \param metric Error metric type
     */
    SimulationEstimator(uint32_t num_patterns, uint32_t seed, ErrorMetric metric);

    void initialize(Ntk const& ntk) override;
    double estimate(LAC const& lac) override;
    uint64_t estimate_count(LAC const& lac) override;
    void update_after_apply(LAC const& lac) override;
    double accumulated_error() const override;
    uint64_t accumulated_error_count() const override;
    ErrorMetric metric() const override;

    double verify_error(Ntk const& original, Ntk const& approximate) override;
    uint64_t verify_error_count(Ntk const& original, Ntk const& approximate) override;

    /*! \brief Get truth table for a node.
     *  \param n Node
     *  \return Truth table
     */
    TT const& get_tt(node n) const override;

    /*! \brief Get number of simulation bits.
     *  \return Number of bits
     */
    uint64_t num_bits() const override;

    /*! \brief Set truth table for a node.
     *  \param n Node
     *  \param tt Truth table
     */
    void set_tt(node n, TT const& tt);

private:
    double compute_error_rate(TT const& target, TT const& candidate) const;
    double compute_mhd(TT const& target, TT const& candidate) const;
    double compute_mse(TT const& target, TT const& candidate) const;

    uint32_t num_patterns_;
    uint32_t seed_;
    ErrorMetric metric_;
    double accumulated_error_;
    uint64_t accumulated_error_count_;
    uint64_t num_bits_;

    Ntk const* ntk_;
    std::unique_ptr<mockturtle::unordered_node_map<TT, Ntk>> tts_;
};

} // namespace lut_synth::approximate
