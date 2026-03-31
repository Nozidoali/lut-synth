#include "lut-synth/approximate/resynthesis/miter-estimator.hpp"

#include <algorithm>

#include <mockturtle/algorithms/simulation.hpp>

#include "lut-synth/approximate/resynthesis/tt-ops.hpp"

namespace lut_synth::approximate {

MiterEstimator::MiterEstimator(uint32_t num_patterns, uint32_t seed,
                               ErrorMetric metric)
    : num_patterns_(num_patterns), seed_(seed), metric_(metric),
      accumulated_error_(0.0), accumulated_error_count_(0), num_bits_(0),
      original_ntk_(nullptr) {}

void MiterEstimator::initialize(Ntk const& ntk) {
    original_ntk_ = &ntk;
    approximate_ntk_ = ntk;
    accumulated_error_ = 0.0;
    accumulated_error_count_ = 0;

    std::mt19937 rng(seed_);
    mockturtle::partial_simulator sim(ntk.num_pis(), num_patterns_, rng());

    app_tts_ = std::make_unique<mockturtle::unordered_node_map<TT, Ntk>>(approximate_ntk_);
    mockturtle::simulate_nodes(approximate_ntk_, *app_tts_, sim);
    num_bits_ = sim.num_bits();

    builder_.build(*original_ntk_, approximate_ntk_, *app_tts_,
                   num_patterns_, seed_, num_bits_, metric_);
}

double MiterEstimator::estimate(LAC const& lac) {
    uint64_t count = estimate_count(lac);
    if (num_bits_ == 0) return 1.0;
    return static_cast<double>(count) / num_bits_;
}

uint64_t MiterEstimator::estimate_count(LAC const& lac) {
    if (!original_ntk_ || builder_.bd_po_to_node().empty()) return num_bits_;

    TT target_tt = (*app_tts_)[lac.target];
    TT candidate_tt = compute_candidate_tt(lac);
    TT is_changed = tt_ops::compute_xor(target_tt, candidate_tt, num_bits_);

    uint64_t delta_error = 0;
    uint32_t num_pos = builder_.num_pos();

    for (uint32_t k = 0; k < num_pos; ++k) {
        TT const& bd = builder_.bd_po_to_node()[k][lac.target];
        TT affects_po = tt_ops::compute_and(is_changed, bd, num_bits_);
        TT miter_correct = tt_ops::compute_not(builder_.po_tts()[k], num_bits_);
        TT new_errors = tt_ops::compute_and(affects_po, miter_correct, num_bits_);

        uint64_t weight = (metric_ == ErrorMetric::ER) ? 1 : (1ULL << k);
        delta_error += tt_ops::count_ones(new_errors, num_bits_) * weight;
    }

    return delta_error;
}

MiterEstimator::TT MiterEstimator::compute_candidate_tt(LAC const& lac) const {
    TT candidate(num_bits_);

    switch (lac.type) {
    case LACType::Const0:
        break;
    case LACType::Const1:
        for (auto& block : candidate._bits) {
            block = ~0ULL;
        }
        break;
    case LACType::Single: {
        node n = approximate_ntk_.get_node(lac.divisor1);
        candidate = (*app_tts_)[n];
        if (approximate_ntk_.is_complemented(lac.divisor1)) {
            candidate = tt_ops::compute_not(candidate, num_bits_);
        }
        break;
    }
    case LACType::TwoInput: {
        node n1 = approximate_ntk_.get_node(lac.divisor1);
        node n2 = approximate_ntk_.get_node(lac.divisor2);
        TT tt1 = (*app_tts_)[n1];
        TT tt2 = (*app_tts_)[n2];
        if (approximate_ntk_.is_complemented(lac.divisor1)) {
            tt1 = tt_ops::compute_not(tt1, num_bits_);
        }
        if (approximate_ntk_.is_complemented(lac.divisor2)) {
            tt2 = tt_ops::compute_not(tt2, num_bits_);
        }

        switch (lac.func) {
        case TwoInputFunc::And:
            candidate = tt_ops::compute_and(tt1, tt2, num_bits_);
            break;
        case TwoInputFunc::Or:
            candidate = tt_ops::compute_not(tt_ops::compute_and(tt_ops::compute_not(tt1, num_bits_), tt_ops::compute_not(tt2, num_bits_), num_bits_), num_bits_);
            break;
        case TwoInputFunc::Xor:
            candidate = tt_ops::compute_xor(tt1, tt2, num_bits_);
            break;
        case TwoInputFunc::Nand:
            candidate = tt_ops::compute_not(tt_ops::compute_and(tt1, tt2, num_bits_), num_bits_);
            break;
        case TwoInputFunc::Nor:
            candidate = tt_ops::compute_and(tt_ops::compute_not(tt1, num_bits_), tt_ops::compute_not(tt2, num_bits_), num_bits_);
            break;
        case TwoInputFunc::Xnor:
            candidate = tt_ops::compute_not(tt_ops::compute_xor(tt1, tt2, num_bits_), num_bits_);
            break;
        }
        break;
    }
    }

    return candidate;
}

void MiterEstimator::update_after_apply(LAC const& lac) {
    accumulated_error_ += lac.error_delta;
    accumulated_error_count_ += lac.error_count;
}

double MiterEstimator::accumulated_error() const {
    return accumulated_error_;
}

uint64_t MiterEstimator::accumulated_error_count() const {
    return accumulated_error_count_;
}

ErrorMetric MiterEstimator::metric() const {
    return metric_;
}

MiterEstimator::TT const& MiterEstimator::get_tt(node n) const {
    return (*app_tts_)[n];
}

uint64_t MiterEstimator::num_bits() const {
    return num_bits_;
}

void MiterEstimator::rebuild_miter(Ntk const& approximate) {
    approximate_ntk_ = approximate;

    std::mt19937 rng(seed_);
    mockturtle::partial_simulator sim(approximate_ntk_.num_pis(), num_patterns_, rng());

    app_tts_ = std::make_unique<mockturtle::unordered_node_map<TT, Ntk>>(approximate_ntk_);
    mockturtle::simulate_nodes(approximate_ntk_, *app_tts_, sim);

    builder_.build(*original_ntk_, approximate_ntk_, *app_tts_,
                   num_patterns_, seed_, num_bits_, metric_);
}

double MiterEstimator::verify_error(Ntk const& original, Ntk const& approximate) {
    uint64_t count = verify_error_count(original, approximate);
    if (num_bits_ == 0) return 0.0;
    return static_cast<double>(count) / num_bits_;
}

uint64_t MiterEstimator::verify_error_count(Ntk const& original,
                                             Ntk const& approximate) {
    if (original.num_pos() != approximate.num_pos()) return num_bits_;
    if (original.num_pos() == 0) return 0;

    std::mt19937 rng(seed_ + 12345);
    mockturtle::partial_simulator sim_orig(original.num_pis(), num_patterns_, rng());
    std::mt19937 rng2(seed_ + 12345);
    mockturtle::partial_simulator sim_approx(approximate.num_pis(), num_patterns_, rng2());

    mockturtle::unordered_node_map<TT, Ntk> tts_orig(original);
    mockturtle::unordered_node_map<TT, Ntk> tts_approx(approximate);

    mockturtle::simulate_nodes(original, tts_orig, sim_orig);
    mockturtle::simulate_nodes(approximate, tts_approx, sim_approx);

    uint64_t error_count = 0;

    switch (metric_) {
    case ErrorMetric::ER: {
        TT any_diff(num_bits_);
        original.foreach_po([&](auto const& f, auto i) {
            node orig_node = original.get_node(f);
            TT orig_tt = tts_orig[orig_node];
            if (original.is_complemented(f)) {
                orig_tt = tt_ops::compute_not(orig_tt, num_bits_);
            }

            approximate.foreach_po([&](auto const& g, auto j) {
                if (i != j) return;
                node approx_node = approximate.get_node(g);
                TT approx_tt = tts_approx[approx_node];
                if (approximate.is_complemented(g)) {
                    approx_tt = tt_ops::compute_not(approx_tt, num_bits_);
                }

                TT diff = tt_ops::compute_xor(orig_tt, approx_tt, num_bits_);
                for (size_t k = 0; k < any_diff._bits.size() && k < diff._bits.size(); ++k) {
                    any_diff._bits[k] |= diff._bits[k];
                }
            });
        });

        error_count = tt_ops::count_ones(any_diff, num_bits_);
        break;
    }
    case ErrorMetric::MHD:
    case ErrorMetric::MSE: {
        original.foreach_po([&](auto const& f, auto i) {
            node orig_node = original.get_node(f);
            TT orig_tt = tts_orig[orig_node];
            if (original.is_complemented(f)) {
                orig_tt = tt_ops::compute_not(orig_tt, num_bits_);
            }

            approximate.foreach_po([&](auto const& g, auto j) {
                if (i != j) return;
                node approx_node = approximate.get_node(g);
                TT approx_tt = tts_approx[approx_node];
                if (approximate.is_complemented(g)) {
                    approx_tt = tt_ops::compute_not(approx_tt, num_bits_);
                }

                TT diff = tt_ops::compute_xor(orig_tt, approx_tt, num_bits_);
                error_count += tt_ops::count_ones(diff, num_bits_);
            });
        });
        break;
    }
    default:
        break;
    }

    return error_count;
}

} // namespace lut_synth::approximate
