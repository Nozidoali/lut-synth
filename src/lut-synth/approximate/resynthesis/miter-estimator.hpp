#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <kitty/partial_truth_table.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/node_map.hpp>

#include "lut-synth/approximate/resynthesis/error-estimator.hpp"

namespace lut_synth::approximate {

/*! \brief Miter-based error estimator using Boolean difference propagation.
 *
 *  Implements the error estimation approach from ResubALS (TCAD 2024).
 *  Builds a miter circuit comparing original and approximate networks,
 *  then uses Boolean differences for fast LAC error estimation.
 *
 *  Key concepts:
 *  - Miter circuit: XOR of original and approximate outputs
 *  - Boolean difference bd_po_to_node[k][n]: patterns where changing node n
 *    affects primary output k
 *  - Error estimation: count patterns where LAC changes propagate to outputs
 *
 *  For ER metric, estimates delta error as patterns where:
 *  - The LAC changes the node value (isChanged)
 *  - The change propagates to some output (via Boolean difference)
 *  - The output is currently correct (miter PO is 0)
 *
 *  Formula: deltaError = sum over k of: |isChanged & bd[k][n] & ~miterPO[k]|
 */
class MiterEstimator : public ErrorEstimator {
public:
    using TT = kitty::partial_truth_table;

    /*! \brief Construct miter estimator.
     *  \param num_patterns Number of simulation patterns
     *  \param seed Random seed for pattern generation
     *  \param metric Error metric (ER, MHD, or MSE)
     */
    MiterEstimator(uint32_t num_patterns, uint32_t seed, ErrorMetric metric);

    /*! \brief Initialize estimator with network.
     *  \param ntk Original network to approximate
     *
     *  Stores reference to original, builds miter, simulates, and
     *  computes Boolean differences from each node to outputs.
     */
    void initialize(Ntk const& ntk) override;

    /*! \brief Estimate error rate for a LAC.
     *  \param lac Local approximate change to evaluate
     *  \return Estimated error as fraction of patterns
     */
    double estimate(LAC const& lac) override;

    /*! \brief Estimate error count for a LAC.
     *  \param lac Local approximate change to evaluate
     *  \return Number of patterns with errors
     *
     *  Uses Boolean differences to compute how many patterns would have
     *  errors if this LAC is applied. Only counts patterns that are
     *  currently correct and would become incorrect.
     */
    uint64_t estimate_count(LAC const& lac) override;

    /*! \brief Update state after applying a LAC.
     *  \param lac Applied LAC
     */
    void update_after_apply(LAC const& lac) override;

    /*! \brief Get accumulated error rate. */
    double accumulated_error() const override;

    /*! \brief Get accumulated error count. */
    uint64_t accumulated_error_count() const override;

    /*! \brief Get error metric type. */
    ErrorMetric metric() const override;

    /*! \brief Get truth table for a node.
     *  \param n Node in approximate network
     *  \return Simulated truth table
     */
    TT const& get_tt(node n) const override;

    /*! \brief Get number of simulation bits. */
    uint64_t num_bits() const override;

    /*! \brief Verify error between original and approximate networks.
     *  \param original Original network
     *  \param approximate Approximate network
     *  \return Actual error rate
     */
    double verify_error(Ntk const& original, Ntk const& approximate) override;

    /*! \brief Verify error count between networks.
     *  \param original Original network
     *  \param approximate Approximate network
     *  \return Actual error count in patterns
     */
    uint64_t verify_error_count(Ntk const& original, Ntk const& approximate) override;

    /*! \brief Rebuild miter after network modification.
     *  \param approximate Current approximate network
     *
     *  Call after applying LACs to update the miter circuit and
     *  recompute Boolean differences.
     */
    void rebuild_miter(Ntk const& approximate);

    /*! \brief Update internal miter after network modification. */
    void update_network(Ntk const& ntk) override { rebuild_miter(ntk); }

private:
    /*! \brief Build miter circuit from original and approximate networks. */
    void build_miter();

    /*! \brief Simulate miter circuit with random patterns. */
    void simulate_miter();

    /*! \brief Compute all Boolean differences. */
    void compute_boolean_differences();

    /*! \brief Compute Boolean difference from cuts to nodes. */
    void compute_bd_cut_to_node();

    /*! \brief Compute Boolean difference from POs to nodes.
     *
     *  For each PO k and node n, computes bd_po_to_node[k][n] which
     *  indicates patterns where changing n affects output k.
     */
    void compute_bd_po_to_node();

    /*! \brief Compute candidate truth table for a LAC. */
    TT compute_candidate_tt(LAC const& lac) const;


    uint32_t num_patterns_;              /*!< Number of simulation patterns */
    uint32_t seed_;                      /*!< Random seed */
    ErrorMetric metric_;                 /*!< Error metric type */
    double accumulated_error_;           /*!< Accumulated error rate */
    uint64_t accumulated_error_count_;   /*!< Accumulated error count */
    uint64_t num_bits_;                  /*!< Total simulation bits */

    Ntk const* original_ntk_;            /*!< Reference to original network */
    Ntk approximate_ntk_;                /*!< Current approximate network */
    Ntk miter_ntk_;                      /*!< Miter circuit */

    std::vector<node> app_to_miter_;     /*!< Map from approximate to miter nodes */
    std::vector<node> miter_to_app_;     /*!< Map from miter to approximate nodes */

    std::unique_ptr<mockturtle::unordered_node_map<TT, Ntk>> app_tts_;   /*!< Approximate network TTs */
    std::unique_ptr<mockturtle::unordered_node_map<TT, Ntk>> miter_tts_; /*!< Miter network TTs */

    std::vector<TT> miter_po_tts_;              /*!< Miter PO truth tables (current error) */
    std::vector<std::vector<TT>> bd_po_to_node_; /*!< Boolean diff: bd[k][n] = patterns where n affects PO k */
};

} // namespace lut_synth::approximate
