#include "lut-synth/approximate/resynthesis/miter-builder.hpp"

#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/views/topo_view.hpp>

#include "lut-synth/approximate/resynthesis/tt-ops.hpp"

namespace lut_synth::approximate {

void MiterBuilder::build(Ntk const& original, Ntk const& approximate,
                         mockturtle::unordered_node_map<TT, Ntk> const& app_tts,
                         uint32_t num_patterns, uint32_t seed, uint64_t num_bits,
                         ErrorMetric metric) {
    num_bits_ = num_bits;
    build_miter(original, approximate, metric);
    simulate_miter(num_patterns, seed);
    compute_bd_po_to_node(approximate, app_tts);
}

uint32_t MiterBuilder::num_pos() const {
    return miter_ntk_.num_pos();
}

void MiterBuilder::build_miter(Ntk const& original, Ntk const& approximate,
                               ErrorMetric metric) {
    miter_ntk_ = Ntk();
    app_to_miter_.clear();
    app_to_miter_.resize(approximate.size(), 0);
    miter_to_app_.clear();

    std::vector<signal> pi_signals;
    for (uint32_t i = 0; i < original.num_pis(); ++i) {
        pi_signals.push_back(miter_ntk_.create_pi());
    }

    mockturtle::unordered_node_map<signal, Ntk> orig_to_miter(original);
    mockturtle::unordered_node_map<signal, Ntk> app_to_miter_sig(approximate);

    original.foreach_pi([&](node n, uint32_t i) {
        orig_to_miter[n] = pi_signals[i];
    });

    approximate.foreach_pi([&](node n, uint32_t i) {
        app_to_miter_sig[n] = pi_signals[i];
        app_to_miter_[n] = miter_ntk_.get_node(pi_signals[i]);
    });

    orig_to_miter[original.get_node(original.get_constant(false))] =
        miter_ntk_.get_constant(false);
    app_to_miter_sig[approximate.get_node(approximate.get_constant(false))] =
        miter_ntk_.get_constant(false);

    mockturtle::topo_view topo_orig(original);
    topo_orig.foreach_node([&](node n) {
        if (original.is_constant(n) || original.is_pi(n)) return;

        std::vector<signal> children;
        original.foreach_fanin(n, [&](signal f) {
            node child_node = original.get_node(f);
            signal child_sig = orig_to_miter[child_node];
            if (original.is_complemented(f)) {
                child_sig = !child_sig;
            }
            children.push_back(child_sig);
        });

        signal new_sig;
        if (original.is_and(n)) {
            new_sig = miter_ntk_.create_and(children[0], children[1]);
        } else {
            new_sig = miter_ntk_.create_xor(children[0], children[1]);
        }
        orig_to_miter[n] = new_sig;
    });

    mockturtle::topo_view topo_app(approximate);
    topo_app.foreach_node([&](node n) {
        if (approximate.is_constant(n) || approximate.is_pi(n)) return;

        std::vector<signal> children;
        approximate.foreach_fanin(n, [&](signal f) {
            node child_node = approximate.get_node(f);
            signal child_sig = app_to_miter_sig[child_node];
            if (approximate.is_complemented(f)) {
                child_sig = !child_sig;
            }
            children.push_back(child_sig);
        });

        signal new_sig;
        if (approximate.is_and(n)) {
            new_sig = miter_ntk_.create_and(children[0], children[1]);
        } else {
            new_sig = miter_ntk_.create_xor(children[0], children[1]);
        }
        app_to_miter_sig[n] = new_sig;
        app_to_miter_[n] = miter_ntk_.get_node(new_sig);
    });

    miter_to_app_.resize(miter_ntk_.size(), 0);
    approximate.foreach_node([&](node n) {
        if (app_to_miter_[n] != 0) {
            miter_to_app_[app_to_miter_[n]] = n;
        }
    });

    switch (metric) {
    case ErrorMetric::ER: {
        signal any_diff = miter_ntk_.get_constant(false);
        for (uint32_t i = 0; i < original.num_pos(); ++i) {
            signal orig_po = original.po_at(i);
            signal app_po = approximate.po_at(i);

            node orig_node = original.get_node(orig_po);
            node app_node = approximate.get_node(app_po);

            signal orig_sig = orig_to_miter[orig_node];
            signal app_sig = app_to_miter_sig[app_node];

            if (original.is_complemented(orig_po)) orig_sig = !orig_sig;
            if (approximate.is_complemented(app_po)) app_sig = !app_sig;

            signal diff = miter_ntk_.create_xor(orig_sig, app_sig);
            any_diff = miter_ntk_.create_or(any_diff, diff);
        }
        miter_ntk_.create_po(any_diff);
        break;
    }
    case ErrorMetric::MHD:
    case ErrorMetric::MSE: {
        for (uint32_t i = 0; i < original.num_pos(); ++i) {
            signal orig_po = original.po_at(i);
            signal app_po = approximate.po_at(i);

            node orig_node = original.get_node(orig_po);
            node app_node = approximate.get_node(app_po);

            signal orig_sig = orig_to_miter[orig_node];
            signal app_sig = app_to_miter_sig[app_node];

            if (original.is_complemented(orig_po)) orig_sig = !orig_sig;
            if (approximate.is_complemented(app_po)) app_sig = !app_sig;

            signal diff = miter_ntk_.create_xor(orig_sig, app_sig);
            miter_ntk_.create_po(diff);
        }
        break;
    }
    default:
        break;
    }
}

void MiterBuilder::simulate_miter(uint32_t num_patterns, uint32_t seed) {
    std::mt19937 rng(seed);
    mockturtle::partial_simulator sim(miter_ntk_.num_pis(), num_patterns, rng());

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

void MiterBuilder::compute_bd_po_to_node(
    Ntk const& approximate,
    mockturtle::unordered_node_map<TT, Ntk> const& app_tts) {
    uint32_t num_pos = miter_ntk_.num_pos();
    bd_po_to_node_.clear();
    bd_po_to_node_.resize(num_pos);

    std::vector<node> topo_order;
    mockturtle::topo_view topo(approximate);
    topo.foreach_node([&](node n) {
        if (!approximate.is_constant(n)) {
            topo_order.push_back(n);
        }
    });

    for (uint32_t po_idx = 0; po_idx < num_pos; ++po_idx) {
        std::vector<TT>& bd_for_po = bd_po_to_node_[po_idx];
        bd_for_po.resize(approximate.size());

        for (size_t i = 0; i < approximate.size(); ++i) {
            bd_for_po[i] = TT(num_bits_);
        }

        approximate.foreach_po([&](signal f, uint32_t i) {
            if (i == po_idx) {
                node n = approximate.get_node(f);
                for (auto& block : bd_for_po[n]._bits) {
                    block = ~0ULL;
                }
            }
        });

        for (auto it = topo_order.rbegin(); it != topo_order.rend(); ++it) {
            node n = *it;
            if (approximate.is_pi(n)) continue;
            if (!approximate.is_and(n) && !approximate.is_xor3(n)) continue;

            TT& bd_n = bd_for_po[n];
            if (tt_ops::count_ones(bd_n, num_bits_) == 0) continue;

            approximate.foreach_fanin(n, [&](signal f) {
                node child = approximate.get_node(f);
                TT child_tt = app_tts[child];
                if (approximate.is_complemented(f)) {
                    child_tt = tt_ops::compute_not(child_tt, num_bits_);
                }

                TT sensitivity(num_bits_);
                if (approximate.is_and(n)) {
                    signal other_fanin;
                    approximate.foreach_fanin(n, [&](signal g) {
                        if (approximate.get_node(g) != child ||
                            approximate.is_complemented(g) != approximate.is_complemented(f)) {
                            other_fanin = g;
                        }
                    });
                    node other_node = approximate.get_node(other_fanin);
                    TT other_tt = app_tts[other_node];
                    if (approximate.is_complemented(other_fanin)) {
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

} // namespace lut_synth::approximate
