#include "lut-synth/approximate/resynthesis/narrow-resub.hpp"

#include <cassert>
#include <limits>
#include <utility>
#include <vector>

#include <mockturtle/algorithms/cleanup.hpp>

#include "lut-synth/approximate/resynthesis/integer-estimator.hpp"
#include "lut-synth/approximate/resynthesis/lac.hpp"

namespace lut_synth::approximate {

namespace {

using Ntk = mockturtle::xag_network;
using node = Ntk::node;
using signal = Ntk::signal;

struct Candidate {
    LAC lac;
    double error_delta;
    int32_t size_gain;
};

double score(int32_t gain, double err_delta) {
    if (err_delta <= 0.0) {
        return static_cast<double>(gain) * 1e12;
    }
    return static_cast<double>(gain) / err_delta;
}

std::pair<signal, signal> get_and_fanins(Ntk const& ntk, node n) {
    signal f0{};
    signal f1{};
    uint32_t idx = 0;
    ntk.foreach_fanin(n, [&](auto const& f) {
        if (idx == 0) {
            f0 = f;
        } else if (idx == 1) {
            f1 = f;
        }
        ++idx;
    });
    assert(idx == 2);
    return {f0, f1};
}

struct EvalContext {
    IntegerEstimator& est;
    double remaining_budget;
    uint32_t max_per_pattern_cap;
    uint32_t& rejected_by_cap;
};

void evaluate_and_push(
    EvalContext& ctx,
    LAC lac,
    int32_t gain,
    std::vector<Candidate>& out) {
    if (gain <= 0) return;
    lac.size_gain = gain;
    double err = ctx.est.estimate(lac);
    if (err > ctx.remaining_budget) return;
    if (ctx.max_per_pattern_cap > 0) {
        uint32_t mx = ctx.est.estimate_max_per_pattern(lac);
        if (mx > ctx.max_per_pattern_cap) {
            ++ctx.rejected_by_cap;
            return;
        }
    }
    lac.error_delta = err;
    out.push_back({lac, err, gain});
}

std::vector<Candidate> build_candidates(
    Ntk const& ntk,
    node target,
    NarrowResubParams const& params,
    EvalContext& ctx) {

    auto [f0, f1] = get_and_fanins(ntk, target);
    int32_t mffc = static_cast<int32_t>(compute_mffc_size(ntk, target));

    std::vector<Candidate> out;
    out.reserve(5);

    if (params.try_const0) {
        evaluate_and_push(ctx, LAC(target, LACType::Const0, mffc, 0.0),
                          mffc, out);
    }
    if (params.try_const1) {
        evaluate_and_push(ctx, LAC(target, LACType::Const1, mffc, 0.0),
                          mffc, out);
    }
    if (params.try_fanin) {
        evaluate_and_push(ctx, LAC(target, f0, mffc, 0.0),
                          mffc, out);
        evaluate_and_push(ctx, LAC(target, f1, mffc, 0.0),
                          mffc, out);
    }
    if (params.try_xor) {
        evaluate_and_push(ctx,
                          LAC(target, f0, f1, TwoInputFunc::Xor, mffc - 1, 0.0),
                          mffc - 1, out);
    }
    return out;
}

uint32_t count_ands(Ntk const& ntk) {
    uint32_t n = 0;
    ntk.foreach_gate([&](auto g) {
        if (ntk.is_and(g)) ++n;
    });
    return n;
}

std::vector<bool> compute_locked_cone(
    Ntk const& ntk, std::vector<bool> const& locked_outputs) {
    std::vector<bool> in_cone(ntk.size(), false);
    if (locked_outputs.empty()) return in_cone;

    std::vector<node> stack;
    uint32_t po_idx = 0;
    ntk.foreach_po([&](auto const& f) {
        if (po_idx < locked_outputs.size() && locked_outputs[po_idx]) {
            node n = ntk.get_node(f);
            if (!ntk.is_constant(n) && !in_cone[n]) {
                in_cone[n] = true;
                stack.push_back(n);
            }
        }
        ++po_idx;
    });

    while (!stack.empty()) {
        node n = stack.back();
        stack.pop_back();
        ntk.foreach_fanin(n, [&](auto const& f) {
            node c = ntk.get_node(f);
            if (ntk.is_constant(c) || ntk.is_pi(c)) return;
            if (!in_cone[c]) {
                in_cone[c] = true;
                stack.push_back(c);
            }
        });
    }
    return in_cone;
}

} // namespace

NarrowResubResult narrow_and_resub(Ntk const& input,
                                   NarrowResubParams const& params) {
    NarrowResubResult result;
    result.network = input;
    result.stats.original_size = result.network.num_gates();
    result.stats.and_before = count_ands(result.network);

    double accumulated = 0.0;
    uint32_t iteration = 0;
    uint32_t const max_iter =
        params.max_iterations == 0 ? std::numeric_limits<uint32_t>::max()
                                   : params.max_iterations;

    while (iteration < max_iter) {
        ++iteration;

        IntegerEstimator estimator(params.num_patterns, params.seed, params.weights);
        if (params.care_patterns.empty()) {
            estimator.initialize(result.network);
        } else {
            estimator.initialize_with_patterns(result.network,
                                                params.care_patterns);
        }

        std::vector<bool> locked_cone =
            compute_locked_cone(result.network, params.locked_outputs);

        Candidate best;
        best.size_gain = 0;
        best.error_delta = 0.0;
        double best_score = 0.0;
        bool found = false;

        std::vector<node> targets;
        uint32_t skipped = 0;
        result.network.foreach_gate([&](auto g) {
            if (!result.network.is_and(g)) return;
            if (g < locked_cone.size() && locked_cone[g]) {
                ++skipped;
                return;
            }
            targets.push_back(g);
        });
        if (iteration == 1) result.stats.and_locked = skipped;

        double remaining = params.error_bound - accumulated;
        if (remaining <= 0.0) break;

        EvalContext ctx{estimator, remaining,
                        params.max_integer_error_per_pattern,
                        result.stats.lacs_rejected_by_cap};

        for (node t : targets) {
            std::vector<Candidate> cands =
                build_candidates(result.network, t, params, ctx);
            for (Candidate const& c : cands) {
                double s = score(c.size_gain, c.error_delta);
                if (s > best_score) {
                    best_score = s;
                    best = c;
                    found = true;
                }
            }
        }

        if (!found) break;

        signal repl = create_replacement(result.network, best.lac);
        result.network.substitute_node(best.lac.target, repl);
        accumulated += best.error_delta;
        ++result.stats.lacs_applied;
    }

    result.network = mockturtle::cleanup_dangling(result.network);
    result.stats.final_size = result.network.num_gates();
    result.stats.and_after = count_ands(result.network);
    result.stats.accumulated_error = accumulated;

    return result;
}

} // namespace lut_synth::approximate
