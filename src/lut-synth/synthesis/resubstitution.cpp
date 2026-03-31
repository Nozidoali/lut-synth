#include "lut-synth/synthesis/synthesis.hpp"

#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/experimental/cost_generic_resub.hpp>
#include <mockturtle/utils/recursive_cost_functions.hpp>

namespace lut_synth {

struct xag_mc_cost : mockturtle::recursive_cost_functions<mockturtle::xag_network> {
    using Ntk = mockturtle::xag_network;
    using node = mockturtle::node<Ntk>;
    using signal = mockturtle::signal<Ntk>;
    using context_t = uint32_t;

    context_t operator()(Ntk const &ntk, node const &n,
                         std::vector<context_t> const &fanin_ctx = {}) const override {
        if (ntk.is_constant(n) || ntk.is_pi(n)) return 0u;

        uint32_t and_count = 0u;
        for (auto ctx : fanin_ctx) and_count += ctx;
        if (ntk.is_and(n)) and_count += 1u;

        return and_count;
    }

    void operator()(Ntk const &ntk, node const &n, uint32_t &total_cost,
                    context_t const) const override {
        if (!ntk.is_constant(n) && !ntk.is_pi(n) && ntk.is_and(n)) {
            total_cost += 1u;
        }
    }
};

mockturtle::xag_network rewrite_with_mc_cost(mockturtle::xag_network const &ntk) {
    mockturtle::xag_network optimized = mockturtle::cleanup_dangling(ntk);

    mockturtle::experimental::cost_generic_resub_params ps;
    ps.wps.max_pis = 8u;
    ps.wps.max_inserts = 3u;
    ps.wps.max_divisors = 150u;
    ps.wps.skip_fanout_limit_for_roots = 1000u;
    ps.wps.use_dont_cares = false;

    mockturtle::experimental::cost_generic_resub_stats st;
    mockturtle::experimental::cost_generic_resub(optimized, xag_mc_cost{}, ps, &st);
    optimized = mockturtle::cleanup_dangling(optimized);

    return optimized;
}

} // namespace lut_synth
