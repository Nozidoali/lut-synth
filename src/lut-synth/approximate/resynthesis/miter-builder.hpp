#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <kitty/partial_truth_table.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/node_map.hpp>

#include "lut-synth/error.hpp"

namespace lut_synth::approximate {

/*! \brief Builds and simulates a miter circuit for error estimation.
 *
 *  Constructs a miter XAG from original and approximate networks,
 *  simulates it with random patterns, and computes Boolean differences
 *  from each node back to the primary outputs.
 */
class MiterBuilder {
public:
    using Ntk = mockturtle::xag_network;
    using node = Ntk::node;
    using signal = Ntk::signal;
    using TT = kitty::partial_truth_table;

    /*! \brief Build miter, simulate, and compute Boolean differences.
     *  \param original Original network
     *  \param approximate Approximate network
     *  \param app_tts Simulated truth tables for approximate network nodes
     *  \param num_patterns Number of simulation patterns
     *  \param seed Random seed for pattern generation
     *  \param num_bits Number of simulation bits
     *  \param metric Error metric type
     */
    void build(Ntk const& original, Ntk const& approximate,
               mockturtle::unordered_node_map<TT, Ntk> const& app_tts,
               uint32_t num_patterns, uint32_t seed, uint64_t num_bits,
               ErrorMetric metric);

    /*! \brief Get miter PO truth tables. */
    std::vector<TT> const& po_tts() const { return miter_po_tts_; }

    /*! \brief Get Boolean difference from POs to nodes. */
    std::vector<std::vector<TT>> const& bd_po_to_node() const { return bd_po_to_node_; }

    /*! \brief Get number of miter POs. */
    uint32_t num_pos() const;

private:
    /*! \brief Build miter circuit from original and approximate networks. */
    void build_miter(Ntk const& original, Ntk const& approximate,
                     ErrorMetric metric);

    /*! \brief Simulate miter circuit with random patterns. */
    void simulate_miter(uint32_t num_patterns, uint32_t seed);

    /*! \brief Compute Boolean difference from POs to nodes. */
    void compute_bd_po_to_node(Ntk const& approximate,
                               mockturtle::unordered_node_map<TT, Ntk> const& app_tts);

    Ntk miter_ntk_;
    std::vector<node> app_to_miter_;
    std::vector<node> miter_to_app_;
    std::unique_ptr<mockturtle::unordered_node_map<TT, Ntk>> miter_tts_;
    std::vector<TT> miter_po_tts_;
    std::vector<std::vector<TT>> bd_po_to_node_;
    uint64_t num_bits_{0};
};

} // namespace lut_synth::approximate
