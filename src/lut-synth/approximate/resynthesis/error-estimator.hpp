#pragma once

#include <kitty/partial_truth_table.hpp>
#include <mockturtle/networks/xag.hpp>

#include "lut-synth/error.hpp"
#include "lut-synth/approximate/resynthesis/lac.hpp"

namespace lut_synth::approximate {

using lut_synth::ErrorMetric;

/*! \brief Abstract interface for error estimation. */
class ErrorEstimator {
public:
    using Ntk = mockturtle::xag_network;
    using node = Ntk::node;
    using signal = Ntk::signal;
    using TT = kitty::partial_truth_table;

    virtual ~ErrorEstimator() = default;

    /*! \brief Initialize estimator with network.
     *  \param ntk Network to analyze
     */
    virtual void initialize(Ntk const& ntk) = 0;

    /*! \brief Estimate error for a LAC.
     *  \param lac LAC to evaluate
     *  \return Estimated error delta
     */
    virtual double estimate(LAC const& lac) = 0;

    /*! \brief Estimate error count (integer) for a LAC.
     *  \param lac LAC to evaluate
     *  \return Number of erroneous patterns
     */
    virtual uint64_t estimate_count(LAC const& lac);

    /*! \brief Update estimator after applying a LAC.
     *  \param lac Applied LAC
     */
    virtual void update_after_apply(LAC const& lac) = 0;

    /*! \brief Get current accumulated error.
     *  \return Total accumulated error
     */
    virtual double accumulated_error() const = 0;

    /*! \brief Get current accumulated error count (integer).
     *  \return Total error count
     */
    virtual uint64_t accumulated_error_count() const;

    /*! \brief Get error metric type.
     *  \return Error metric
     */
    virtual ErrorMetric metric() const = 0;

    /*! \brief Get truth table for a node.
     *  \param n Node
     *  \return Truth table
     */
    virtual TT const& get_tt(node n) const = 0;

    /*! \brief Get number of simulation bits.
     *  \return Number of bits
     */
    virtual uint64_t num_bits() const = 0;

    /*! \brief Compute actual error between two networks via fresh simulation.
     *  \param original Original network
     *  \param approximate Approximate network
     *  \return Actual error rate
     */
    virtual double verify_error(Ntk const& original, Ntk const& approximate);

    /*! \brief Compute actual error count between two networks.
     *  \param original Original network
     *  \param approximate Approximate network
     *  \return Number of erroneous patterns
     */
    virtual uint64_t verify_error_count(Ntk const& original, Ntk const& approximate);

    /*! \brief Update estimator after the network has been modified.
     *
     *  Called when the approximate network is changed outside of LAC
     *  application (e.g. cleanup_dangling). Subclasses that maintain
     *  internal state tied to a specific network should override.
     *  Default is a no-op.
     *
     *  \param ntk The updated network
     */
    virtual void update_network(Ntk const& ntk) { (void)ntk; }
};

} // namespace lut_synth::approximate
