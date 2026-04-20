#include "lut-synth/approximate/resynthesis/integer-estimator.hpp"
#include "lut-synth/approximate/resynthesis/tt-ops.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <random>

namespace lut_synth::approximate {

IntegerEstimator::IntegerEstimator(uint32_t num_patterns, uint32_t seed,
                                   std::vector<double> const& weights,
                                   uint32_t exhaustive_threshold)
    : num_patterns_(num_patterns), seed_(seed),
      exhaustive_threshold_(exhaustive_threshold), weights_(weights),
      accumulated_error_(0.0), accumulated_error_count_(0), num_bits_(0),
      ntk_(nullptr) {}

void IntegerEstimator::initialize(Ntk const& ntk) {
    ntk_ = &ntk;
    accumulated_error_ = 0.0;
    accumulated_error_count_ = 0;

    uint32_t const n = ntk.num_pis();
    tts_ = std::make_unique<mockturtle::unordered_node_map<TT, Ntk>>(ntk);

    if (n > 0 && n <= exhaustive_threshold_) {
        uint64_t const num_bits = 1ull << n;
        std::vector<kitty::partial_truth_table> patterns;
        patterns.reserve(n);
        for (uint32_t i = 0; i < n; ++i) {
            kitty::partial_truth_table p(num_bits);
            for (uint64_t b = 0; b < num_bits; ++b) {
                if ((b >> i) & 1ull) kitty::set_bit(p, b);
            }
            patterns.push_back(std::move(p));
        }
        mockturtle::partial_simulator sim(patterns);
        mockturtle::simulate_nodes(ntk, *tts_, sim);
        num_bits_ = num_bits;
    } else {
        std::mt19937 rng(seed_);
        mockturtle::partial_simulator sim(n, num_patterns_, rng());
        mockturtle::simulate_nodes(ntk, *tts_, sim);
        num_bits_ = sim.num_bits();
    }

    po_nodes_.clear();
    po_complemented_.clear();
    ntk.foreach_po([&](auto const& f) {
        po_nodes_.push_back(ntk.get_node(f));
        po_complemented_.push_back(ntk.is_complemented(f));
    });

    uint32_t num_outputs = po_nodes_.size();
    exact_integers_.resize(num_bits_);

    for (uint64_t x = 0; x < num_bits_; ++x) {
        uint32_t value = 0;
        for (uint32_t i = 0; i < num_outputs; ++i) {
            TT const& tt = (*tts_)[po_nodes_[i]];
            bool bit = (tt._bits[x >> 6] >> (x & 0x3f)) & 1;
            if (po_complemented_[i]) bit = !bit;
            if (bit) {
                value |= (1u << (num_outputs - 1 - i));
            }
        }
        exact_integers_[x] = value;
    }
}

IntegerEstimator::TT IntegerEstimator::compute_candidate_tt(LAC const& lac) const {
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

    return candidate;
}

namespace {

inline bool bit_at(IntegerEstimator::TT const& tt, uint64_t x) {
    return (tt._bits[x >> 6] >> (x & 0x3f)) & 1u;
}

uint64_t popcount_bits(IntegerEstimator::TT const& tt) {
    uint64_t c = 0;
    for (uint64_t block : tt._bits) c += __builtin_popcountll(block);
    return c;
}

} // namespace

std::vector<uint32_t> IntegerEstimator::collect_affected_outputs(LAC const& lac) const {
    std::vector<uint32_t> affected;
    uint32_t num_outputs = po_nodes_.size();
    for (uint32_t i = 0; i < num_outputs; ++i) {
        if (po_nodes_[i] == lac.target) affected.push_back(i);
    }
    return affected;
}

uint32_t IntegerEstimator::new_value_at(
    uint64_t x, TT const& candidate,
    std::vector<bool> const& is_affected_po) const {
    uint32_t num_outputs = po_nodes_.size();
    uint32_t new_value = 0;
    for (uint32_t i = 0; i < num_outputs; ++i) {
        bool bit = is_affected_po[i]
                       ? bit_at(candidate, x)
                       : bit_at((*tts_)[po_nodes_[i]], x);
        if (po_complemented_[i]) bit = !bit;
        if (bit) new_value |= (1u << (num_outputs - 1 - i));
    }
    return new_value;
}

template <typename Visit>
void IntegerEstimator::for_each_changed_pattern(LAC const& lac, Visit visit) const {
    TT target_tt = (*tts_)[lac.target];
    TT candidate = compute_candidate_tt(lac);
    TT diff_mask = tt_ops::compute_xor(target_tt, candidate, num_bits_);

    std::vector<uint32_t> affected = collect_affected_outputs(lac);
    if (affected.empty()) {
        visit(false, candidate, diff_mask, std::vector<bool>{});
        return;
    }

    std::vector<bool> is_affected_po(po_nodes_.size(), false);
    for (uint32_t ai : affected) is_affected_po[ai] = true;
    visit(true, candidate, diff_mask, is_affected_po);
}

double IntegerEstimator::compute_integer_error_for_lac(LAC const& lac) const {
    double result = 0.0;
    bool uniform = weights_.empty();
    for_each_changed_pattern(lac, [&](bool affects_po, TT const&,
                                      TT const& diff_mask,
                                      std::vector<bool> const& is_affected_po) {
        if (!affects_po) {
            result = static_cast<double>(popcount_bits(diff_mask)) / num_bits_;
            return;
        }
        TT target_tt = (*tts_)[lac.target];
        TT candidate = compute_candidate_tt(lac);
        for (uint64_t x = 0; x < num_bits_; ++x) {
            if (!bit_at(diff_mask, x)) continue;
            uint32_t new_value = new_value_at(x, candidate, is_affected_po);
            int32_t abs_diff = std::abs(static_cast<int32_t>(new_value) -
                                        static_cast<int32_t>(exact_integers_[x]));
            double w = uniform ? (1.0 / num_bits_) : weights_[x];
            result += w * abs_diff;
        }
    });
    return result;
}

uint64_t IntegerEstimator::compute_integer_error_count_for_lac(LAC const& lac) const {
    uint64_t result = 0;
    for_each_changed_pattern(lac, [&](bool affects_po, TT const&,
                                      TT const& diff_mask,
                                      std::vector<bool> const& is_affected_po) {
        if (!affects_po) {
            result = popcount_bits(diff_mask);
            return;
        }
        TT candidate = compute_candidate_tt(lac);
        for (uint64_t x = 0; x < num_bits_; ++x) {
            if (!bit_at(diff_mask, x)) continue;
            uint32_t new_value = new_value_at(x, candidate, is_affected_po);
            if (new_value != exact_integers_[x]) ++result;
        }
    });
    return result;
}

double IntegerEstimator::estimate(LAC const& lac) {
    if (!ntk_ || !tts_) return 1.0;
    return compute_integer_error_for_lac(lac);
}

uint64_t IntegerEstimator::estimate_count(LAC const& lac) {
    if (!ntk_ || !tts_) return num_bits_;
    return compute_integer_error_count_for_lac(lac);
}

uint32_t IntegerEstimator::estimate_max_per_pattern(LAC const& lac) {
    if (!ntk_ || !tts_) return 0;
    return compute_max_integer_error_for_lac(lac);
}

uint32_t IntegerEstimator::compute_max_integer_error_for_lac(LAC const& lac) const {
    uint32_t result = 0;
    for_each_changed_pattern(lac, [&](bool affects_po, TT const&,
                                      TT const& diff_mask,
                                      std::vector<bool> const& is_affected_po) {
        if (!affects_po) return; // internal target: no per-pattern max info
        TT candidate = compute_candidate_tt(lac);
        for (uint64_t x = 0; x < num_bits_; ++x) {
            if (!bit_at(diff_mask, x)) continue;
            uint32_t new_value = new_value_at(x, candidate, is_affected_po);
            uint32_t abs_diff = static_cast<uint32_t>(std::abs(
                static_cast<int32_t>(new_value) -
                static_cast<int32_t>(exact_integers_[x])));
            if (abs_diff > result) result = abs_diff;
        }
    });
    return result;
}

void IntegerEstimator::update_after_apply(LAC const& lac) {
    accumulated_error_ += lac.error_delta;
    accumulated_error_count_ += lac.error_count;
}

double IntegerEstimator::accumulated_error() const {
    return accumulated_error_;
}

uint64_t IntegerEstimator::accumulated_error_count() const {
    return accumulated_error_count_;
}

ErrorMetric IntegerEstimator::metric() const {
    return ErrorMetric::ER;
}

IntegerEstimator::TT const& IntegerEstimator::get_tt(node n) const {
    return (*tts_)[n];
}

uint64_t IntegerEstimator::num_bits() const {
    return num_bits_;
}

} // namespace lut_synth::approximate
