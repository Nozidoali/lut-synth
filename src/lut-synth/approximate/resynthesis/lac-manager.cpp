#include "lut-synth/approximate/resynthesis/lac-manager.hpp"
#include "lut-synth/approximate/resynthesis/tt-ops.hpp"

#include <algorithm>

namespace lut_synth::approximate {

LACManager::LACManager(Ntk const& ntk, ErrorEstimator& estimator,
                       uint32_t max_divisors, uint32_t max_lac_size)
    : ntk_(ntk), estimator_(estimator), max_divisors_(max_divisors),
      max_lac_size_(max_lac_size), current_target_(0), current_mffc_(0),
      num_on_bits_(0), num_off_bits_(0), num_bits_(0) {
    collect_divisors();
    num_bits_ = estimator_.num_bits();
}

void LACManager::collect_divisors() {
    ntk_.foreach_pi([&](auto n) { divisors_.push_back(n); });

    std::vector<node> gates;
    ntk_.foreach_gate([&](auto n) { gates.push_back(n); });

    for (auto n : gates) {
        if (divisors_.size() >= max_divisors_) break;
        divisors_.push_back(n);
    }
}

std::vector<LACManager::node> LACManager::get_candidates() const {
    std::vector<node> candidates;
    ntk_.foreach_gate([&](auto n) {
        if (ntk_.is_and(n)) {
            candidates.push_back(n);
        }
    });
    return candidates;
}

std::vector<LACManager::node> const& LACManager::divisors() const {
    return divisors_;
}

void LACManager::prepare_target(node target) {
    current_target_ = target;
    current_mffc_ = static_cast<int32_t>(compute_mffc_size(ntk_, target));

    target_tt_ = estimator_.get_tt(target);
    on_set_ = target_tt_;
    off_set_ = tt_ops::compute_not(target_tt_, num_bits_);

    num_on_bits_ = tt_ops::count_ones(on_set_, num_bits_);
    num_off_bits_ = tt_ops::count_ones(off_set_, num_bits_);
}

void LACManager::classify_divisors(double error_budget) {
    pos_unate_divs_.clear();
    neg_unate_divs_.clear();
    binate_divs_.clear();

    for (auto div : divisors_) {
        if (div == current_target_) continue;

        TT div_tt = estimator_.get_tt(div);

        uint64_t pos_fp = tt_ops::count_ones(tt_ops::compute_and(div_tt, off_set_, num_bits_), num_bits_);
        uint64_t pos_fn = tt_ops::count_ones(tt_ops::compute_and(tt_ops::compute_not(div_tt, num_bits_), on_set_, num_bits_), num_bits_);
        double pos_error = static_cast<double>(pos_fp + pos_fn) / static_cast<double>(num_bits_);

        TT neg_div_tt = tt_ops::compute_not(div_tt, num_bits_);
        uint64_t neg_fp = tt_ops::count_ones(tt_ops::compute_and(neg_div_tt, off_set_, num_bits_), num_bits_);
        uint64_t neg_fn = tt_ops::count_ones(tt_ops::compute_and(tt_ops::compute_not(neg_div_tt, num_bits_), on_set_, num_bits_), num_bits_);
        double neg_error = static_cast<double>(neg_fp + neg_fn) / static_cast<double>(num_bits_);

        signal sig_pos = ntk_.make_signal(div);
        signal sig_neg = !sig_pos;

        bool pos_valid = pos_error <= error_budget;
        bool neg_valid = neg_error <= error_budget;

        if (pos_valid) {
            uint64_t coverage = tt_ops::count_ones(tt_ops::compute_and(div_tt, on_set_, num_bits_), num_bits_);
            pos_unate_divs_.push_back({div, sig_pos, sig_neg, coverage, pos_error});
        }
        if (neg_valid) {
            uint64_t coverage = tt_ops::count_ones(tt_ops::compute_and(neg_div_tt, on_set_, num_bits_), num_bits_);
            neg_unate_divs_.push_back({div, sig_pos, sig_neg, coverage, neg_error});
        }
        if (!pos_valid && !neg_valid) {
            binate_divs_.push_back({div, sig_pos, sig_neg, 0, std::min(pos_error, neg_error)});
        }
    }

    auto cmp = [](DivisorInfo const& a, DivisorInfo const& b) {
        if (a.error != b.error) return a.error < b.error;
        return a.coverage > b.coverage;
    };
    std::sort(pos_unate_divs_.begin(), pos_unate_divs_.end(), cmp);
    std::sort(neg_unate_divs_.begin(), neg_unate_divs_.end(), cmp);
    std::sort(binate_divs_.begin(), binate_divs_.end(), cmp);
}

double LACManager::compute_error_fast(TT const& candidate) const {
    uint64_t fp = tt_ops::count_ones(tt_ops::compute_and(candidate, off_set_, num_bits_), num_bits_);
    uint64_t fn = tt_ops::count_ones(tt_ops::compute_and(tt_ops::compute_not(candidate, num_bits_), on_set_, num_bits_), num_bits_);
    return static_cast<double>(fp + fn) / static_cast<double>(num_bits_);
}

std::vector<LAC> LACManager::generate_lacs(node target, double error_budget) {
    std::vector<LAC> lacs;

    prepare_target(target);
    classify_divisors(error_budget);

    find_const_lacs(error_budget, lacs);
    if (max_lac_size_ >= 1) {
        find_single_lacs(error_budget, lacs);
    }
    if (max_lac_size_ >= 2) {
        find_two_input_lacs(error_budget, lacs);
    }

    return lacs;
}

LAC LACManager::find_best_lac(node target, double error_budget) {
    std::vector<LAC> lacs = generate_lacs(target, error_budget);

    LAC best;
    for (auto const& lac : lacs) {
        if (lac.is_valid() && lac.error_delta <= error_budget) {
            if (!best.is_valid() || lac.benefit_ratio() > best.benefit_ratio()) {
                best = lac;
            }
        }
    }
    return best;
}

void LACManager::find_const_lacs(double error_budget, std::vector<LAC>& lacs) {
    if (current_mffc_ <= 0) return;

    double error0 = static_cast<double>(num_on_bits_) / static_cast<double>(num_bits_);
    if (error0 <= error_budget) {
        LAC lac0(current_target_, LACType::Const0, current_mffc_, error0);
        lacs.push_back(lac0);
    }

    double error1 = static_cast<double>(num_off_bits_) / static_cast<double>(num_bits_);
    if (error1 <= error_budget) {
        LAC lac1(current_target_, LACType::Const1, current_mffc_, error1);
        lacs.push_back(lac1);
    }
}

void LACManager::find_single_lacs(double error_budget, std::vector<LAC>& lacs) {
    if (current_mffc_ <= 0) return;

    for (auto const& info : pos_unate_divs_) {
        if (info.error > error_budget) break;
        LAC lac(current_target_, info.sig_pos, current_mffc_, info.error);
        lacs.push_back(lac);
    }

    for (auto const& info : neg_unate_divs_) {
        if (info.error > error_budget) break;
        LAC lac(current_target_, info.sig_neg, current_mffc_, info.error);
        lacs.push_back(lac);
    }
}

void LACManager::find_two_input_lacs(double error_budget, std::vector<LAC>& lacs) {
    int32_t size_gain = current_mffc_ - 1;
    if (size_gain <= 0) return;

    size_t max_binates = std::min(binate_divs_.size(), size_t(50));

    for (size_t i = 0; i < max_binates; ++i) {
        TT tt1 = estimator_.get_tt(binate_divs_[i].div);

        for (size_t j = i + 1; j < max_binates; ++j) {
            TT tt2 = estimator_.get_tt(binate_divs_[j].div);

            bool neg1_opts[] = {false, true};
            bool neg2_opts[] = {false, true};

            for (bool neg1 : neg1_opts) {
                TT op1 = neg1 ? tt_ops::compute_not(tt1, num_bits_) : tt1;
                for (bool neg2 : neg2_opts) {
                    TT op2 = neg2 ? tt_ops::compute_not(tt2, num_bits_) : tt2;

                    TT and_tt = tt_ops::compute_and(op1, op2, num_bits_);
                    double and_error = compute_error_fast(and_tt);
                    if (and_error <= error_budget) {
                        signal s1 = neg1 ? binate_divs_[i].sig_neg : binate_divs_[i].sig_pos;
                        signal s2 = neg2 ? binate_divs_[j].sig_neg : binate_divs_[j].sig_pos;
                        LAC lac(current_target_, s1, s2, TwoInputFunc::And, size_gain, and_error);
                        lacs.push_back(lac);
                    }

                    TT or_tt = tt_ops::compute_not(tt_ops::compute_and(tt_ops::compute_not(op1, num_bits_), tt_ops::compute_not(op2, num_bits_), num_bits_), num_bits_);
                    double or_error = compute_error_fast(or_tt);
                    if (or_error <= error_budget) {
                        signal s1 = neg1 ? binate_divs_[i].sig_neg : binate_divs_[i].sig_pos;
                        signal s2 = neg2 ? binate_divs_[j].sig_neg : binate_divs_[j].sig_pos;
                        LAC lac(current_target_, s1, s2, TwoInputFunc::Or, size_gain, or_error);
                        lacs.push_back(lac);
                    }
                }
            }

            TT xor_tt = tt_ops::compute_xor(tt1, tt2, num_bits_);
            double xor_error = compute_error_fast(xor_tt);
            if (xor_error <= error_budget) {
                LAC lac(current_target_, binate_divs_[i].sig_pos, binate_divs_[j].sig_pos,
                        TwoInputFunc::Xor, size_gain, xor_error);
                lacs.push_back(lac);
            }

            TT xnor_tt = tt_ops::compute_not(xor_tt, num_bits_);
            double xnor_error = compute_error_fast(xnor_tt);
            if (xnor_error <= error_budget) {
                LAC lac(current_target_, binate_divs_[i].sig_pos, binate_divs_[j].sig_pos,
                        TwoInputFunc::Xnor, size_gain, xnor_error);
                lacs.push_back(lac);
            }
        }
    }

    for (auto const& pos_info : pos_unate_divs_) {
        for (auto const& binate_info : binate_divs_) {
            if (pos_info.div == binate_info.div) continue;

            TT tt1 = estimator_.get_tt(pos_info.div);
            TT tt2 = estimator_.get_tt(binate_info.div);

            for (bool neg2 : {false, true}) {
                TT op2 = neg2 ? tt_ops::compute_not(tt2, num_bits_) : tt2;
                TT and_tt = tt_ops::compute_and(tt1, op2, num_bits_);
                double and_error = compute_error_fast(and_tt);
                if (and_error <= error_budget) {
                    signal s2 = neg2 ? binate_info.sig_neg : binate_info.sig_pos;
                    LAC lac(current_target_, pos_info.sig_pos, s2,
                            TwoInputFunc::And, size_gain, and_error);
                    lacs.push_back(lac);
                }
            }
        }
    }

    for (auto const& neg_info : neg_unate_divs_) {
        for (auto const& binate_info : binate_divs_) {
            if (neg_info.div == binate_info.div) continue;

            TT tt1 = tt_ops::compute_not(estimator_.get_tt(neg_info.div), num_bits_);
            TT tt2 = estimator_.get_tt(binate_info.div);

            for (bool neg2 : {false, true}) {
                TT op2 = neg2 ? tt_ops::compute_not(tt2, num_bits_) : tt2;
                TT and_tt = tt_ops::compute_and(tt1, op2, num_bits_);
                double and_error = compute_error_fast(and_tt);
                if (and_error <= error_budget) {
                    signal s2 = neg2 ? binate_info.sig_neg : binate_info.sig_pos;
                    LAC lac(current_target_, neg_info.sig_neg, s2,
                            TwoInputFunc::And, size_gain, and_error);
                    lacs.push_back(lac);
                }
            }
        }
    }
}

} // namespace lut_synth::approximate
