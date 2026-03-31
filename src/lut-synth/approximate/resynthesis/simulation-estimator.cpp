#include "lut-synth/approximate/resynthesis/simulation-estimator.hpp"
#include "lut-synth/approximate/resynthesis/tt-ops.hpp"

#include <algorithm>

namespace lut_synth::approximate {

SimulationEstimator::SimulationEstimator(uint32_t num_patterns, uint32_t seed,
                                         ErrorMetric metric)
    : num_patterns_(num_patterns), seed_(seed), metric_(metric),
      accumulated_error_(0.0), accumulated_error_count_(0), num_bits_(0),
      ntk_(nullptr) {}

void SimulationEstimator::initialize(Ntk const& ntk) {
    ntk_ = &ntk;
    accumulated_error_ = 0.0;
    accumulated_error_count_ = 0;

    std::mt19937 rng(seed_);
    mockturtle::partial_simulator sim(ntk.num_pis(), num_patterns_, rng());

    tts_ = std::make_unique<mockturtle::unordered_node_map<TT, Ntk>>(ntk);
    mockturtle::simulate_nodes(ntk, *tts_, sim);
    num_bits_ = sim.num_bits();
}

double SimulationEstimator::estimate(LAC const& lac) {
    if (!ntk_ || !tts_) return 1.0;

    TT target = (*tts_)[lac.target];
    if (target.num_bits() != num_bits_) return 1.0;

    TT candidate;

    switch (lac.type) {
    case LACType::Const0: {
        candidate = TT(num_bits_);
        break;
    }
    case LACType::Const1: {
        candidate = TT(num_bits_);
        for (auto& block : candidate._bits) {
            block = ~0ULL;
        }
        break;
    }
    case LACType::Single: {
        node node1 = ntk_->get_node(lac.divisor1);
        TT tt1 = (*tts_)[node1];
        if (ntk_->is_complemented(lac.divisor1)) {
            tt1 = tt_ops::compute_not(tt1, num_bits_);
        }
        candidate = tt1;
        break;
    }
    case LACType::TwoInput: {
        node node1 = ntk_->get_node(lac.divisor1);
        node node2 = ntk_->get_node(lac.divisor2);
        TT tt1 = (*tts_)[node1];
        TT tt2 = (*tts_)[node2];
        if (ntk_->is_complemented(lac.divisor1)) {
            tt1 = tt_ops::compute_not(tt1, num_bits_);
        }
        if (ntk_->is_complemented(lac.divisor2)) {
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

    switch (metric_) {
    case ErrorMetric::ER:
        return compute_error_rate(target, candidate);
    case ErrorMetric::MHD:
        return compute_mhd(target, candidate);
    case ErrorMetric::MSE:
        return compute_mse(target, candidate);
    default:
        break;
    }
    return 1.0;
}

uint64_t SimulationEstimator::estimate_count(LAC const& lac) {
    if (!ntk_ || !tts_) return num_bits_;

    TT target = (*tts_)[lac.target];
    if (target.num_bits() != num_bits_) return num_bits_;

    TT candidate;

    switch (lac.type) {
    case LACType::Const0: {
        candidate = TT(num_bits_);
        break;
    }
    case LACType::Const1: {
        candidate = TT(num_bits_);
        for (auto& block : candidate._bits) {
            block = ~0ULL;
        }
        break;
    }
    case LACType::Single: {
        node node1 = ntk_->get_node(lac.divisor1);
        TT tt1 = (*tts_)[node1];
        if (ntk_->is_complemented(lac.divisor1)) {
            tt1 = tt_ops::compute_not(tt1, num_bits_);
        }
        candidate = tt1;
        break;
    }
    case LACType::TwoInput: {
        node node1 = ntk_->get_node(lac.divisor1);
        node node2 = ntk_->get_node(lac.divisor2);
        TT tt1 = (*tts_)[node1];
        TT tt2 = (*tts_)[node2];
        if (ntk_->is_complemented(lac.divisor1)) {
            tt1 = tt_ops::compute_not(tt1, num_bits_);
        }
        if (ntk_->is_complemented(lac.divisor2)) {
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

    return tt_ops::count_diff_bits(target, candidate, num_bits_);
}

void SimulationEstimator::update_after_apply(LAC const& lac) {
    accumulated_error_ += lac.error_delta;
    accumulated_error_count_ += lac.error_count;
}

double SimulationEstimator::accumulated_error() const {
    return accumulated_error_;
}

uint64_t SimulationEstimator::accumulated_error_count() const {
    return accumulated_error_count_;
}

ErrorMetric SimulationEstimator::metric() const {
    return metric_;
}

double SimulationEstimator::verify_error(Ntk const& original, Ntk const& approximate) {
    uint64_t count = verify_error_count(original, approximate);
    if (num_bits_ == 0) return 0.0;
    return static_cast<double>(count) / num_bits_;
}

uint64_t SimulationEstimator::verify_error_count(Ntk const& original,
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

        uint64_t remaining = num_bits_;
        for (size_t k = 0; k < any_diff._bits.size() && remaining > 0; ++k) {
            uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
            uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
            error_count += __builtin_popcountll(any_diff._bits[k] & mask);
            remaining -= bits_in_chunk;
        }
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

                error_count += tt_ops::count_diff_bits(orig_tt, approx_tt, num_bits_);
            });
        });
        break;
    }
    default:
        break;
    }

    return error_count;
}

SimulationEstimator::TT const& SimulationEstimator::get_tt(node n) const {
    return (*tts_)[n];
}

uint64_t SimulationEstimator::num_bits() const {
    return num_bits_;
}

void SimulationEstimator::set_tt(node n, TT const& tt) {
    (*tts_)[n] = tt;
}

double SimulationEstimator::compute_error_rate(TT const& target,
                                                TT const& candidate) const {
    if (target.num_bits() != candidate.num_bits() || target.num_bits() == 0) {
        return 1.0;
    }
    return static_cast<double>(tt_ops::count_diff_bits(target, candidate, num_bits_)) /
           static_cast<double>(num_bits_);
}

double SimulationEstimator::compute_mhd(TT const& target,
                                         TT const& candidate) const {
    return static_cast<double>(tt_ops::count_diff_bits(target, candidate, num_bits_));
}

double SimulationEstimator::compute_mse(TT const& target,
                                         TT const& candidate) const {
    uint64_t diff = tt_ops::count_diff_bits(target, candidate, num_bits_);
    return static_cast<double>(diff * diff) /
           static_cast<double>(num_bits_ * num_bits_);
}

} // namespace lut_synth::approximate
