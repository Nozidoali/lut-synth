#include "lut-synth/resynthesis/cost-generic-resub.hpp"
#include "lut-synth/resynthesis/resub-patterns.hpp"
#include "lut-synth/resynthesis/resynthesis-util.hpp"

#include <chrono>
#include <unordered_set>
#include <vector>

#include <kitty/kitty.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/reconv_cut.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/views/topo_view.hpp>

namespace lut_synth {

namespace {

using node = xag_network::node;
using signal = xag_network::signal;
using TT = kitty::dynamic_truth_table;

TT compute_gate_tt(xag_network const& ntk, node n, size_t num_vars,
                   mockturtle::unordered_node_map<TT, xag_network>& tts) {
    TT left(num_vars), right(num_vars);
    ntk.foreach_fanin(n, [&](auto f, auto i) {
        node fn = ntk.get_node(f);
        TT val;
        if (ntk.is_constant(fn)) {
            val = ntk.constant_value(fn) ? ~kitty::create<TT>(num_vars)
                                         : kitty::create<TT>(num_vars);
        } else {
            val = ntk.is_complemented(f) ? ~tts[fn] : tts[fn];
        }
        if (i == 0) left = val;
        else right = val;
    });
    return ntk.is_and(n) ? (left & right) : (left ^ right);
}

void simulate_nodes(xag_network const& ntk, std::vector<node> const& nodes,
                    std::vector<node> const& leaves,
                    mockturtle::unordered_node_map<TT, xag_network>& tts) {
    mockturtle::default_simulator<TT> sim(leaves.size());
    for (size_t i = 0; i < leaves.size(); ++i) {
        tts[leaves[i]] = sim.compute_pi(i);
    }
    for (auto n : nodes) {
        if (tts.has(n)) continue;
        bool can_compute = true;
        ntk.foreach_fanin(n, [&](auto f) {
            node fn = ntk.get_node(f);
            if (!tts.has(fn) && !ntk.is_constant(fn)) can_compute = false;
        });
        if (can_compute) tts[n] = compute_gate_tt(ntk, n, leaves.size(), tts);
    }
}

void collect_mffc(xag_network const& ntk,
                  mockturtle::fanout_view<xag_network> const& fv, node n,
                  std::unordered_set<node> const& boundary,
                  std::unordered_set<node>& mffc) {
    if (boundary.count(n) || ntk.is_constant(n) || ntk.is_pi(n)) return;
    if (fv.fanout_size(n) > 1) return;
    mffc.insert(n);
    ntk.foreach_fanin(
        n, [&](auto f) { collect_mffc(ntk, fv, ntk.get_node(f), boundary, mffc); });
}

void collect_divisors(xag_network const& ntk,
                      mockturtle::fanout_view<xag_network> const& fv, node root,
                      std::vector<node> const& leaves,
                      std::unordered_set<node> const& mffc,
                      CostGenericResubParams const& ps,
                      std::vector<node>& divisors) {
    std::unordered_set<node> leaf_set(leaves.begin(), leaves.end());
    std::unordered_set<node> visited;
    for (auto l : leaves) {
        divisors.push_back(l);
        visited.insert(l);
    }
    mockturtle::topo_view topo(ntk);
    topo.foreach_gate([&](auto n) {
        if (n == root || visited.count(n) || mffc.count(n)) return;
        if (fv.fanout_size(n) > ps.skip_fanout_limit_for_divisors) return;
        if (divisors.size() >= ps.max_divisors) return;
        bool all_fanins_visited = true;
        ntk.foreach_fanin(n, [&](auto f) {
            node fn = ntk.get_node(f);
            if (!visited.count(fn) && !ntk.is_constant(fn) && !ntk.is_pi(fn))
                all_fanins_visited = false;
        });
        if (!all_fanins_visited) return;
        bool has_leaf = false;
        ntk.foreach_fanin(n, [&](auto f) {
            if (leaf_set.count(ntk.get_node(f)) || visited.count(ntk.get_node(f)))
                has_leaf = true;
        });
        if (has_leaf) {
            divisors.push_back(n);
            visited.insert(n);
        }
    });
}

uint32_t compute_mffc_cost(std::unordered_set<node> const& mffc,
                           xag_network const& ntk) {
    uint32_t cost = 0;
    for (auto n : mffc) {
        if (ntk.is_and(n)) ++cost;
    }
    return cost;
}

class ResubEngine {
    xag_network& ntk_;
    CostGenericResubParams const& ps_;
    CostGenericResubStats& st_;
    mockturtle::fanout_view<xag_network> fv_;

public:
    ResubEngine(xag_network& ntk, CostGenericResubParams const& ps,
                CostGenericResubStats& st)
        : ntk_(ntk), ps_(ps), st_(st), fv_(ntk) {}

    void run() {
        using clock = std::chrono::high_resolution_clock;
        clock::time_point start = clock::now();
        st_.initial_size = count_ands(ntk_);
        std::vector<node> nodes;
        mockturtle::topo_view topo(ntk_);
        topo.foreach_gate([&](auto n) { nodes.push_back(n); });
        for (auto n : nodes) {
            if (ntk_.is_dead(n)) continue;
            if (fv_.fanout_size(n) > ps_.skip_fanout_limit_for_roots) continue;
            process_node(n);
        }
        clock::time_point end = clock::now();
        st_.time_total_ms =
            std::chrono::duration<double, std::milli>(end - start).count();
    }

private:
    void process_node(node n) {
        mockturtle::reconvergence_driven_cut_parameters cut_ps;
        cut_ps.max_leaves = ps_.max_pis;
        auto [leaves, _] = mockturtle::reconvergence_driven_cut<
            xag_network, false, false>(ntk_, {n}, cut_ps);
        if (leaves.empty() || leaves.size() > 10) return;

        std::unordered_set<node> leaf_set(leaves.begin(), leaves.end());
        std::unordered_set<node> mffc;
        collect_mffc(ntk_, fv_, n, leaf_set, mffc);
        uint32_t mffc_cost = compute_mffc_cost(mffc, ntk_);
        if (mffc_cost == 0) return;

        std::vector<node> divisors;
        collect_divisors(ntk_, fv_, n, leaves, mffc, ps_, divisors);
        std::vector<node> mffc_vec(mffc.begin(), mffc.end());
        mockturtle::unordered_node_map<TT, xag_network> tts(ntk_);
        simulate_nodes(ntk_, divisors, leaves, tts);
        simulate_nodes(ntk_, mffc_vec, leaves, tts);
        if (!tts.has(n)) return;

        st_.num_windows++;
        ResynContext ctx{ntk_, divisors, tts, tts[n], mffc_cost};
        if (auto replacement = find_replacement(ctx)) {
            ntk_.substitute_node(n, *replacement);
            st_.num_substitutions++;
        }
    }
};

}  // namespace

CostGenericResubResult cost_generic_resub_mc(xag_network const& ntk,
                                              CostGenericResubParams const& ps) {
    xag_network result = ntk.clone();
    CostGenericResubStats stats;
    ResubEngine engine(result, ps, stats);
    engine.run();
    result = mockturtle::cleanup_dangling(result);
    return CostGenericResubResult{result, stats};
}

}  // namespace lut_synth
