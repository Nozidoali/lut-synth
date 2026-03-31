#pragma once

#include <optional>
#include <vector>

#include <kitty/kitty.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/node_map.hpp>

namespace lut_synth {

/*! \brief Context for resubstitution pattern matching. */
struct ResynContext {
    using node = mockturtle::xag_network::node;
    using signal = mockturtle::xag_network::signal;
    using TT = kitty::dynamic_truth_table;

    mockturtle::xag_network& ntk;
    std::vector<node> const& divisors;
    mockturtle::unordered_node_map<TT, mockturtle::xag_network>& tts;
    TT const& target;
    uint32_t mffc_cost;

    /*! \brief Create signal from node. */
    signal make_sig(node n) const { return ntk.make_signal(n); }

    /*! \brief Create complemented signal from node. */
    signal make_not(node n) const { return ntk.create_not(make_sig(n)); }

    /*! \brief Get truth table for node. */
    TT const& tt(node n) const { return tts[n]; }

    /*! \brief Check if truth table exists for node. */
    bool has_tt(node n) const { return tts.has(n); }
};

/*! \brief Search for a replacement signal using resubstitution patterns.
 *
 *  Tries wire, xor2, xor3, and2, xor-and, and3 patterns in order.
 */
std::optional<mockturtle::xag_network::signal> find_replacement(ResynContext const& ctx);

} // namespace lut_synth
