#include "lut-synth/approximate/resynthesis/miter-estimator.hpp"

#include <algorithm>

#include <mockturtle/algorithms/simulation.hpp>

#include "lut-synth/approximate/resynthesis/tt-ops.hpp"
#include <mockturtle/views/topo_view.hpp>

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

    build_miter();
    simulate_miter();
    compute_boolean_differences();
}

void MiterEstimator::build_miter() {
    miter_ntk_ = Ntk();
    app_to_miter_.clear();
    app_to_miter_.resize(approximate_ntk_.size(), 0);
    miter_to_app_.clear();

    std::vector<signal> pi_signals;
    for (uint32_t i = 0; i < original_ntk_->num_pis(); ++i) {
        pi_signals.push_back(miter_ntk_.create_pi());
    }

    mockturtle::unordered_node_map<signal, Ntk> orig_to_miter(*original_ntk_);
    mockturtle::unordered_node_map<signal, Ntk> app_to_miter_sig(approximate_ntk_);

    original_ntk_->foreach_pi([&](node n, uint32_t i) {
        orig_to_miter[n] = pi_signals[i];
    });

    approximate_ntk_.foreach_pi([&](node n, uint32_t i) {
        app_to_miter_sig[n] = pi_signals[i];
        app_to_miter_[n] = miter_ntk_.get_node(pi_signals[i]);
    });

    orig_to_miter[original_ntk_->get_node(original_ntk_->get_constant(false))] =
        miter_ntk_.get_constant(false);
    app_to_miter_sig[approximate_ntk_.get_node(approximate_ntk_.get_constant(false))] =
        miter_ntk_.get_constant(false);

    mockturtle::topo_view topo_orig(*original_ntk_);
    topo_orig.foreach_node([&](node n) {
        if (original_ntk_->is_constant(n) || original_ntk_->is_pi(n)) return;

        std::vector<signal> children;
        original_ntk_->foreach_fanin(n, [&](signal f) {
            node child_node = original_ntk_->get_node(f);
            signal child_sig = orig_to_miter[child_node];
            if (original_ntk_->is_complemented(f)) {
                child_sig = !child_sig;
            }
            children.push_back(child_sig);
        });

        signal new_sig;
        if (original_ntk_->is_and(n)) {
            new_sig = miter_ntk_.create_and(children[0], children[1]);
        } else {
            new_sig = miter_ntk_.create_xor(children[0], children[1]);
        }
        orig_to_miter[n] = new_sig;
    });

    mockturtle::topo_view topo_app(approximate_ntk_);
    topo_app.foreach_node([&](node n) {
        if (approximate_ntk_.is_constant(n) || approximate_ntk_.is_pi(n)) return;

        std::vector<signal> children;
        approximate_ntk_.foreach_fanin(n, [&](signal f) {
            node child_node = approximate_ntk_.get_node(f);
            signal child_sig = app_to_miter_sig[child_node];
            if (approximate_ntk_.is_complemented(f)) {
                child_sig = !child_sig;
            }
            children.push_back(child_sig);
        });

        signal new_sig;
        if (approximate_ntk_.is_and(n)) {
            new_sig = miter_ntk_.create_and(children[0], children[1]);
        } else {
            new_sig = miter_ntk_.create_xor(children[0], children[1]);
        }
        app_to_miter_sig[n] = new_sig;
        app_to_miter_[n] = miter_ntk_.get_node(new_sig);
    });

    miter_to_app_.resize(miter_ntk_.size(), 0);
    approximate_ntk_.foreach_node([&](node n) {
        if (app_to_miter_[n] != 0) {
            miter_to_app_[app_to_miter_[n]] = n;
        }
    });

    switch (metric_) {
    case ErrorMetric::ER: {
        signal any_diff = miter_ntk_.get_constant(false);
        for (uint32_t i = 0; i < original_ntk_->num_pos(); ++i) {
            signal orig_po = original_ntk_->po_at(i);
            signal app_po = approximate_ntk_.po_at(i);

            node orig_node = original_ntk_->get_node(orig_po);
            node app_node = approximate_ntk_.get_node(app_po);

            signal orig_sig = orig_to_miter[orig_node];
            signal app_sig = app_to_miter_sig[app_node];

            if (original_ntk_->is_complemented(orig_po)) orig_sig = !orig_sig;
            if (approximate_ntk_.is_complemented(app_po)) app_sig = !app_sig;

            signal diff = miter_ntk_.create_xor(orig_sig, app_sig);
            any_diff = miter_ntk_.create_or(any_diff, diff);
        }
        miter_ntk_.create_po(any_diff);
        break;
    }
    case ErrorMetric::MHD:
    case ErrorMetric::MSE: {
        for (uint32_t i = 0; i < original_ntk_->num_pos(); ++i) {
            signal orig_po = original_ntk_->po_at(i);
            signal app_po = approximate_ntk_.po_at(i);

            node orig_node = original_ntk_->get_node(orig_po);
            node app_node = approximate_ntk_.get_node(app_po);

            signal orig_sig = orig_to_miter[orig_node];
            signal app_sig = app_to_miter_sig[app_node];

            if (original_ntk_->is_complemented(orig_po)) orig_sig = !orig_sig;
            if (approximate_ntk_.is_complemented(app_po)) app_sig = !app_sig;

            signal diff = miter_ntk_.create_xor(orig_sig, app_sig);
            miter_ntk_.create_po(diff);
        }
        break;
    }
    default:
        break;
    }
}

void MiterEstimator::simulate_miter() {
    std::mt19937 rng(seed_);
    mockturtle::partial_simulator sim(miter_ntk_.num_pis(), num_patterns_, rng());

    miter_tts_ = std::make_unique<mockturtle::unordered_node_map<TT, Ntk>>(miter_ntk_);
    mockturtle::simulate_nodes(miter_ntk_, *miter_tts_, sim);

    miter_po_tts_.clear();
    miter_ntk_.foreach_po([&](signal f) {
        node n = miter_ntk_.get_node(f);
        TT tt = (*miter_tts_)[n];
        if (miter_ntk_.is_complemented(f)) {
            tt = tt_ops::compute_not(tt, num_bits_);
        }
        miter_po_tts_.push_back(tt);
    });
}

void MiterEstimator::compute_boolean_differences() {
    compute_bd_po_to_node();
}

void MiterEstimator::compute_bd_po_to_node() {
    uint32_t num_pos = miter_ntk_.num_pos();
    bd_po_to_node_.clear();
    bd_po_to_node_.resize(num_pos);

    std::vector<node> topo_order;
    mockturtle::topo_view topo(approximate_ntk_);
    topo.foreach_node([&](node n) {
        if (!approximate_ntk_.is_constant(n)) {
            topo_order.push_back(n);
        }
    });

    for (uint32_t po_idx = 0; po_idx < num_pos; ++po_idx) {
        std::vector<TT>& bd_for_po = bd_po_to_node_[po_idx];
        bd_for_po.resize(approximate_ntk_.size());

        for (size_t i = 0; i < approximate_ntk_.size(); ++i) {
            bd_for_po[i] = TT(num_bits_);
        }

        approximate_ntk_.foreach_po([&](signal f, uint32_t i) {
            if (i == po_idx) {
                node n = approximate_ntk_.get_node(f);
                for (auto& block : bd_for_po[n]._bits) {
                    block = ~0ULL;
                }
            }
        });

        for (auto it = topo_order.rbegin(); it != topo_order.rend(); ++it) {
            node n = *it;
            if (approximate_ntk_.is_pi(n)) continue;
            if (!approximate_ntk_.is_and(n) && !approximate_ntk_.is_xor3(n)) continue;

            TT& bd_n = bd_for_po[n];
            if (tt_ops::count_ones(bd_n, num_bits_) == 0) continue;

            approximate_ntk_.foreach_fanin(n, [&](signal f) {
                node child = approximate_ntk_.get_node(f);
                TT child_tt = (*app_tts_)[child];
                if (approximate_ntk_.is_complemented(f)) {
                    child_tt = tt_ops::compute_not(child_tt, num_bits_);
                }

                TT sensitivity(num_bits_);
                if (approximate_ntk_.is_and(n)) {
                    signal other_fanin;
                    approximate_ntk_.foreach_fanin(n, [&](signal g) {
                        if (approximate_ntk_.get_node(g) != child ||
                            approximate_ntk_.is_complemented(g) != approximate_ntk_.is_complemented(f)) {
                            other_fanin = g;
                        }
                    });
                    node other_node = approximate_ntk_.get_node(other_fanin);
                    TT other_tt = (*app_tts_)[other_node];
                    if (approximate_ntk_.is_complemented(other_fanin)) {
                        other_tt = tt_ops::compute_not(other_tt, num_bits_);
                    }
                    sensitivity = other_tt;
                } else {
                    for (auto& block : sensitivity._bits) {
                        block = ~0ULL;
                    }
                }

                TT propagated = tt_ops::compute_and(bd_n, sensitivity, num_bits_);
                bd_for_po[child] = tt_ops::compute_xor(bd_for_po[child], propagated, num_bits_);
                for (size_t i = 0; i < bd_for_po[child]._bits.size(); ++i) {
                    bd_for_po[child]._bits[i] |= propagated._bits[i];
                }
            });
        }
    }
}

double MiterEstimator::estimate(LAC const& lac) {
    uint64_t count = estimate_count(lac);
    if (num_bits_ == 0) return 1.0;
    return static_cast<double>(count) / num_bits_;
}

uint64_t MiterEstimator::estimate_count(LAC const& lac) {
    if (!original_ntk_ || bd_po_to_node_.empty()) return num_bits_;

    TT target_tt = (*app_tts_)[lac.target];
    TT candidate_tt = compute_candidate_tt(lac);
    TT is_changed = tt_ops::compute_xor(target_tt, candidate_tt, num_bits_);

    uint64_t delta_error = 0;
    uint32_t num_pos = miter_ntk_.num_pos();

    for (uint32_t k = 0; k < num_pos; ++k) {
        TT& bd = bd_po_to_node_[k][lac.target];
        TT affects_po = tt_ops::compute_and(is_changed, bd, num_bits_);
        TT miter_correct = tt_ops::compute_not(miter_po_tts_[k], num_bits_);
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

    build_miter();
    simulate_miter();
    compute_boolean_differences();
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
