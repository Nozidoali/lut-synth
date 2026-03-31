#include "lut-synth/approximate/resynthesis/integer-estimator.hpp"
#include "lut-synth/approximate/resynthesis/tt-ops.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <random>

namespace lut_synth::approximate {

IntegerEstimator::IntegerEstimator(uint32_t num_patterns, uint32_t seed,
                                   std::vector<double> const& weights)
    : num_patterns_(num_patterns), seed_(seed), weights_(weights),
      accumulated_error_(0.0), accumulated_error_count_(0), num_bits_(0),
      ntk_(nullptr) {}

void IntegerEstimator::initialize(Ntk const& ntk) {
    ntk_ = &ntk;
    accumulated_error_ = 0.0;
    accumulated_error_count_ = 0;

    std::mt19937 rng(seed_);
    mockturtle::partial_simulator sim(ntk.num_pis(), num_patterns_, rng());

    tts_ = std::make_unique<mockturtle::unordered_node_map<TT, Ntk>>(ntk);
    mockturtle::simulate_nodes(ntk, *tts_, sim);
    num_bits_ = sim.num_bits();

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

double IntegerEstimator::compute_integer_error_for_lac(LAC const& lac) const {
    TT target_tt = (*tts_)[lac.target];
    TT candidate = compute_candidate_tt(lac);

    TT diff_mask = tt_ops::compute_xor(target_tt, candidate, num_bits_);

    uint32_t num_outputs = po_nodes_.size();
    bool affects_output = false;
    std::vector<uint32_t> affected_output_indices;

    for (uint32_t i = 0; i < num_outputs; ++i) {
        if (po_nodes_[i] == lac.target) {
            affects_output = true;
            affected_output_indices.push_back(i);
        }
    }

    if (!affects_output) {
        uint64_t diff_count = 0;
        for (size_t k = 0; k < diff_mask._bits.size(); ++k) {
            diff_count += __builtin_popcountll(diff_mask._bits[k]);
        }
        return static_cast<double>(diff_count) / num_bits_;
    }

    double total_error = 0.0;
    bool uniform = weights_.empty();

    for (uint64_t x = 0; x < num_bits_; ++x) {
        bool bit_changed = (diff_mask._bits[x >> 6] >> (x & 0x3f)) & 1;
        if (!bit_changed) continue;

        uint32_t new_value = 0;
        for (uint32_t i = 0; i < num_outputs; ++i) {
            bool bit;
            bool is_affected = false;
            for (uint32_t ai : affected_output_indices) {
                if (ai == i) { is_affected = true; break; }
            }

            if (is_affected) {
                bit = (candidate._bits[x >> 6] >> (x & 0x3f)) & 1;
                if (po_complemented_[i]) bit = !bit;
            } else {
                TT const& tt = (*tts_)[po_nodes_[i]];
                bit = (tt._bits[x >> 6] >> (x & 0x3f)) & 1;
                if (po_complemented_[i]) bit = !bit;
            }

            if (bit) {
                new_value |= (1u << (num_outputs - 1 - i));
            }
        }

        int32_t abs_diff = std::abs(static_cast<int32_t>(new_value) -
                                    static_cast<int32_t>(exact_integers_[x]));
        double w = uniform ? (1.0 / num_bits_) : weights_[x];
        total_error += w * abs_diff;
    }

    return total_error;
}

uint64_t IntegerEstimator::compute_integer_error_count_for_lac(LAC const& lac) const {
    TT target_tt = (*tts_)[lac.target];
    TT candidate = compute_candidate_tt(lac);
    TT diff_mask = tt_ops::compute_xor(target_tt, candidate, num_bits_);

    uint32_t num_outputs = po_nodes_.size();
    bool affects_output = false;
    std::vector<uint32_t> affected_output_indices;

    for (uint32_t i = 0; i < num_outputs; ++i) {
        if (po_nodes_[i] == lac.target) {
            affects_output = true;
            affected_output_indices.push_back(i);
        }
    }

    if (!affects_output) {
        uint64_t diff_count = 0;
        for (size_t k = 0; k < diff_mask._bits.size(); ++k) {
            diff_count += __builtin_popcountll(diff_mask._bits[k]);
        }
        return diff_count;
    }

    uint64_t error_count = 0;
    for (uint64_t x = 0; x < num_bits_; ++x) {
        bool bit_changed = (diff_mask._bits[x >> 6] >> (x & 0x3f)) & 1;
        if (!bit_changed) continue;

        uint32_t new_value = 0;
        for (uint32_t i = 0; i < num_outputs; ++i) {
            bool bit;
            bool is_affected = false;
            for (uint32_t ai : affected_output_indices) {
                if (ai == i) { is_affected = true; break; }
            }

            if (is_affected) {
                bit = (candidate._bits[x >> 6] >> (x & 0x3f)) & 1;
                if (po_complemented_[i]) bit = !bit;
            } else {
                TT const& tt = (*tts_)[po_nodes_[i]];
                bit = (tt._bits[x >> 6] >> (x & 0x3f)) & 1;
                if (po_complemented_[i]) bit = !bit;
            }

            if (bit) {
                new_value |= (1u << (num_outputs - 1 - i));
            }
        }

        if (new_value != exact_integers_[x]) {
            ++error_count;
        }
    }

    return error_count;
}

double IntegerEstimator::estimate(LAC const& lac) {
    if (!ntk_ || !tts_) return 1.0;
    return compute_integer_error_for_lac(lac);
}

uint64_t IntegerEstimator::estimate_count(LAC const& lac) {
    if (!ntk_ || !tts_) return num_bits_;
    return compute_integer_error_count_for_lac(lac);
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
