#include "lut-synth/resynthesis/cost-generic-resub.hpp"
#include "lut-synth/resynthesis/resynthesis-util.hpp"

#include <chrono>
#include <optional>
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

struct ResynContext {
    xag_network& ntk;
    std::vector<node> const& divisors;
    mockturtle::unordered_node_map<TT, xag_network>& tts;
    TT const& target;
    uint32_t mffc_cost;

    signal make_sig(node n) const { return ntk.make_signal(n); }
    signal make_not(node n) const { return ntk.create_not(make_sig(n)); }
    TT const& tt(node n) const { return tts[n]; }
    bool has_tt(node n) const { return tts.has(n); }
};

std::optional<signal> find_wire(ResynContext const& ctx) {
    for (auto d : ctx.divisors) {
        if (!ctx.has_tt(d)) continue;
        if (ctx.tt(d) == ctx.target) return ctx.make_sig(d);
        if (~ctx.tt(d) == ctx.target) return ctx.make_not(d);
    }
    return std::nullopt;
}

std::optional<signal> find_xor2(ResynContext const& ctx) {
    for (size_t i = 0; i < ctx.divisors.size(); ++i) {
        if (!ctx.has_tt(ctx.divisors[i])) continue;
        for (size_t j = i + 1; j < ctx.divisors.size(); ++j) {
            if (!ctx.has_tt(ctx.divisors[j])) continue;
            TT xor_tt = ctx.tt(ctx.divisors[i]) ^ ctx.tt(ctx.divisors[j]);
            if (xor_tt == ctx.target) {
                return ctx.ntk.create_xor(ctx.make_sig(ctx.divisors[i]),
                                          ctx.make_sig(ctx.divisors[j]));
            }
            if (~xor_tt == ctx.target) {
                return ctx.ntk.create_not(
                    ctx.ntk.create_xor(ctx.make_sig(ctx.divisors[i]),
                                       ctx.make_sig(ctx.divisors[j])));
            }
        }
    }
    return std::nullopt;
}

std::optional<signal> find_xor3(ResynContext const& ctx) {
    for (size_t i = 0; i < ctx.divisors.size(); ++i) {
        if (!ctx.has_tt(ctx.divisors[i])) continue;
        for (size_t j = i + 1; j < ctx.divisors.size(); ++j) {
            if (!ctx.has_tt(ctx.divisors[j])) continue;
            for (size_t k = j + 1; k < ctx.divisors.size(); ++k) {
                if (!ctx.has_tt(ctx.divisors[k])) continue;
                TT xor_tt = ctx.tt(ctx.divisors[i]) ^ ctx.tt(ctx.divisors[j]) ^
                            ctx.tt(ctx.divisors[k]);
                if (xor_tt == ctx.target || ~xor_tt == ctx.target) {
                    signal xor1 = ctx.ntk.create_xor(
                        ctx.make_sig(ctx.divisors[i]),
                        ctx.make_sig(ctx.divisors[j]));
                    signal xor2 =
                        ctx.ntk.create_xor(xor1, ctx.make_sig(ctx.divisors[k]));
                    return (xor_tt == ctx.target) ? xor2
                                                  : ctx.ntk.create_not(xor2);
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<signal> find_and2(ResynContext const& ctx) {
    if (ctx.mffc_cost < 2) return std::nullopt;
    for (size_t i = 0; i < ctx.divisors.size(); ++i) {
        if (!ctx.has_tt(ctx.divisors[i])) continue;
        TT const& ti = ctx.tt(ctx.divisors[i]);
        for (size_t j = i + 1; j < ctx.divisors.size(); ++j) {
            if (!ctx.has_tt(ctx.divisors[j])) continue;
            TT const& tj = ctx.tt(ctx.divisors[j]);
            if ((ti & tj) == ctx.target)
                return ctx.ntk.create_and(ctx.make_sig(ctx.divisors[i]),
                                          ctx.make_sig(ctx.divisors[j]));
            if ((ti & ~tj) == ctx.target)
                return ctx.ntk.create_and(ctx.make_sig(ctx.divisors[i]),
                                          ctx.make_not(ctx.divisors[j]));
            if ((~ti & tj) == ctx.target)
                return ctx.ntk.create_and(ctx.make_not(ctx.divisors[i]),
                                          ctx.make_sig(ctx.divisors[j]));
            if ((~ti & ~tj) == ctx.target)
                return ctx.ntk.create_and(ctx.make_not(ctx.divisors[i]),
                                          ctx.make_not(ctx.divisors[j]));
        }
    }
    return std::nullopt;
}

std::optional<signal> find_xor_and(ResynContext const& ctx) {
    if (ctx.mffc_cost < 2) return std::nullopt;
    for (size_t i = 0; i < ctx.divisors.size(); ++i) {
        if (!ctx.has_tt(ctx.divisors[i])) continue;
        for (size_t j = i + 1; j < ctx.divisors.size(); ++j) {
            if (!ctx.has_tt(ctx.divisors[j])) continue;
            TT xor_tt =
                ctx.tt(ctx.divisors[i]) ^ ctx.tt(ctx.divisors[j]);
            signal xor_s = ctx.ntk.create_xor(ctx.make_sig(ctx.divisors[i]),
                                               ctx.make_sig(ctx.divisors[j]));
            for (size_t k = 0; k < ctx.divisors.size(); ++k) {
                if (k == i || k == j || !ctx.has_tt(ctx.divisors[k])) continue;
                TT const& tk = ctx.tt(ctx.divisors[k]);
                if ((xor_tt & tk) == ctx.target)
                    return ctx.ntk.create_and(xor_s,
                                              ctx.make_sig(ctx.divisors[k]));
                if ((xor_tt & ~tk) == ctx.target)
                    return ctx.ntk.create_and(xor_s,
                                              ctx.make_not(ctx.divisors[k]));
                if ((~xor_tt & tk) == ctx.target)
                    return ctx.ntk.create_and(ctx.ntk.create_not(xor_s),
                                              ctx.make_sig(ctx.divisors[k]));
                if ((~xor_tt & ~tk) == ctx.target)
                    return ctx.ntk.create_and(ctx.ntk.create_not(xor_s),
                                              ctx.make_not(ctx.divisors[k]));
            }
        }
    }
    return std::nullopt;
}

std::optional<signal> find_and3(ResynContext const& ctx) {
    if (ctx.mffc_cost < 3) return std::nullopt;
    for (size_t i = 0; i < ctx.divisors.size(); ++i) {
        if (!ctx.has_tt(ctx.divisors[i])) continue;
        TT const& ti = ctx.tt(ctx.divisors[i]);
        for (size_t j = i + 1; j < ctx.divisors.size(); ++j) {
            if (!ctx.has_tt(ctx.divisors[j])) continue;
            TT const& tj = ctx.tt(ctx.divisors[j]);
            TT or_tt = ti | tj;
            TT and_tt = ti & tj;
            for (size_t k = 0; k < ctx.divisors.size(); ++k) {
                if (k == i || k == j || !ctx.has_tt(ctx.divisors[k])) continue;
                TT const& tk = ctx.tt(ctx.divisors[k]);
                if ((or_tt & tk) == ctx.target) {
                    signal or_s = ctx.ntk.create_and(
                        ctx.make_not(ctx.divisors[i]),
                        ctx.make_not(ctx.divisors[j]));
                    return ctx.ntk.create_and(ctx.ntk.create_not(or_s),
                                              ctx.make_sig(ctx.divisors[k]));
                }
                if ((and_tt & tk) == ctx.target) {
                    signal and_s = ctx.ntk.create_and(
                        ctx.make_sig(ctx.divisors[i]),
                        ctx.make_sig(ctx.divisors[j]));
                    return ctx.ntk.create_and(and_s,
                                              ctx.make_sig(ctx.divisors[k]));
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<signal> find_replacement(ResynContext const& ctx) {
    if (auto s = find_wire(ctx)) return s;
    if (auto s = find_xor2(ctx)) return s;
    if (auto s = find_xor3(ctx)) return s;
    if (auto s = find_and2(ctx)) return s;
    if (auto s = find_xor_and(ctx)) return s;
    if (auto s = find_and3(ctx)) return s;
    return std::nullopt;
}

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
