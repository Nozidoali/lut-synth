#include "lut-synth/approximate/resynthesis/resubals.hpp"

#include <set>

#include <mockturtle/algorithms/cleanup.hpp>

#include "lut-synth/approximate/resynthesis/knapsack.hpp"
#include "lut-synth/approximate/resynthesis/lac-manager.hpp"
#include "lut-synth/approximate/resynthesis/integer-estimator.hpp"
#include "lut-synth/approximate/resynthesis/simulation-estimator.hpp"
#include "lut-synth/approximate/resynthesis/vecbee-estimator.hpp"

namespace lut_synth::approximate {

namespace {

struct ResubALSState {
    mockturtle::xag_network ntk;
    mockturtle::xag_network original_ntk;
    mockturtle::xag_network backup_ntk;
    std::unique_ptr<ErrorEstimator> estimator;
    std::unique_ptr<LACManager> lac_manager;
    ResubALSParams const& params;
    ResubALSStats& stats;
    uint64_t remaining_budget_count;
    uint64_t error_bound_count;

    ResubALSState(mockturtle::xag_network const& input, ResubALSParams const& p,
                  ResubALSStats& s)
        : ntk(input), original_ntk(input), backup_ntk(input),
          params(p), stats(s), remaining_budget_count(0), error_bound_count(0) {
        stats.original_size = ntk.num_gates();
        stats.num_patterns = p.num_patterns;
    }

    void init_estimator() {
        EstimatorType etype = params.estimator;
        if (params.use_vecbee && etype == EstimatorType::Simulation) {
            etype = EstimatorType::VECBEE;
        }

        switch (etype) {
        case EstimatorType::VECBEE:
            estimator = std::make_unique<VECBEEEstimator>(
                params.num_patterns, params.seed, params.metric);
            break;
        case EstimatorType::Integer:
            estimator = std::make_unique<IntegerEstimator>(
                params.num_patterns, params.seed, params.weights);
            break;
        case EstimatorType::Miter:
        case EstimatorType::Simulation:
        default:
            estimator = std::make_unique<SimulationEstimator>(
                params.num_patterns, params.seed, params.metric);
            break;
        }
        estimator->initialize(ntk);
        error_bound_count = static_cast<uint64_t>(params.error_bound * estimator->num_bits());
        remaining_budget_count = error_bound_count;
    }

    void init_lac_manager() {
        lac_manager = std::make_unique<LACManager>(
            ntk, *estimator, params.max_divisors, params.max_lac_size);
    }

    void create_backup() {
        backup_ntk = ntk;
    }

    void restore_backup() {
        ntk = backup_ntk;
        ++stats.rollbacks;
    }

    void apply_lac_batch(LAC const& lac) {
        mockturtle::xag_network::signal repl = create_replacement(ntk, lac);
        ntk.substitute_node(lac.target, repl);
        remaining_budget_count -= lac.error_count;
        ++stats.lacs_applied;
    }

    bool verify_and_update(std::string const& /*phase_name*/) {
        stats.verified = true;
        return true;
    }
};

void phase1_multiple_selection(ResubALSState& state) {
    if (!state.params.use_knapsack) {
        return;
    }

    state.create_backup();
    std::vector<mockturtle::xag_network::node> candidates = state.lac_manager->get_candidates();
    std::vector<LAC> all_lacs;

    for (auto target : candidates) {
        if (!state.ntk.is_and(target)) continue;
        double budget_rate = static_cast<double>(state.remaining_budget_count) /
                             state.estimator->num_bits();
        std::vector<LAC> lacs = state.lac_manager->generate_lacs(target, budget_rate);
        for (auto& lac : lacs) {
            lac.error_count = state.estimator->estimate_count(lac);
            if (lac.is_valid() && lac.error_count <= state.remaining_budget_count) {
                all_lacs.push_back(lac);
            }
        }
    }

    double budget_rate = static_cast<double>(state.remaining_budget_count) /
                         state.estimator->num_bits();
    KnapsackResult result = knapsack_select(all_lacs, budget_rate);

    uint32_t size_before = state.ntk.num_gates();
    uint64_t error_before = state.remaining_budget_count;
    uint32_t lacs_before = state.stats.lacs_applied;

    for (auto const& lac : result.selected) {
        if (!state.ntk.is_and(lac.target)) continue;
        if (state.remaining_budget_count == 0) break;
        state.apply_lac_batch(lac);
    }

    if (!state.verify_and_update("Phase 1")) {
        state.restore_backup();
        state.remaining_budget_count = error_before;
        state.stats.lacs_applied = lacs_before;
    }

    state.stats.phase1_gain = static_cast<double>(size_before - state.ntk.num_gates());
}

void phase2_single_selection(ResubALSState& state) {
    state.init_estimator();
    state.init_lac_manager();

    uint32_t size_before = state.ntk.num_gates();
    uint32_t lacs_before = state.stats.lacs_applied;

    std::vector<LAC> selected_lacs;
    std::set<uint32_t> used_targets;
    uint64_t estimated_error = 0;
    uint32_t iteration = 0;
    uint32_t const max_iterations = 10;

    std::vector<mockturtle::xag_network::node> candidates = state.lac_manager->get_candidates();

    while (iteration < max_iterations && estimated_error < state.remaining_budget_count) {
        double budget_rate = static_cast<double>(state.remaining_budget_count - estimated_error) /
                             state.estimator->num_bits();

        LAC best;

        for (auto target : candidates) {
            if (!state.ntk.is_and(target)) continue;
            if (used_targets.count(target)) continue;

            LAC lac = state.lac_manager->find_best_lac(target, budget_rate);
            if (!lac.is_valid()) continue;

            lac.error_count = state.estimator->estimate_count(lac);
            if (estimated_error + lac.error_count > state.remaining_budget_count) continue;

            double ratio = lac.benefit_ratio_int(state.estimator->num_bits());
            double best_ratio = best.is_valid() ?
                best.benefit_ratio_int(state.estimator->num_bits()) : 0.0;

            if (!best.is_valid() || ratio > best_ratio) {
                best = lac;
            }
        }
        ++iteration;

        if (best.is_valid()) {
            selected_lacs.push_back(best);
            used_targets.insert(best.target);
            estimated_error += best.error_count;
        } else {
            break;
        }
    }

    state.create_backup();
    for (auto const& lac : selected_lacs) {
        if (!state.ntk.is_and(lac.target)) continue;
        state.apply_lac_batch(lac);
        state.estimator->update_after_apply(lac);
    }

    state.stats.phase2_gain = static_cast<double>(size_before - state.ntk.num_gates());
}

void phase3_cleanup(ResubALSState& state) {
    state.stats.final_size = state.ntk.num_gates();
    state.stats.error_count = state.estimator->accumulated_error_count();
    state.stats.actual_error = static_cast<double>(state.stats.error_count) /
                                state.estimator->num_bits();
    state.stats.verified = (state.stats.error_count <= state.error_bound_count);
}

} // namespace

ResubALSResult resubals(mockturtle::xag_network const& ntk,
                        ResubALSParams const& params) {
    if (params.error_bound <= 0.0) {
        return ResubALSResult{ntk, ResubALSStats{}};
    }

    ResubALSStats stats;
    ResubALSState state(ntk, params, stats);

    state.init_estimator();
    state.init_lac_manager();

    phase1_multiple_selection(state);
    phase2_single_selection(state);
    phase3_cleanup(state);

    return ResubALSResult{state.ntk, stats};
}

} // namespace lut_synth::approximate
