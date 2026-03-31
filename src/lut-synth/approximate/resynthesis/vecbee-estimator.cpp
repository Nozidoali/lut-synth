#include "lut-synth/approximate/resynthesis/vecbee-estimator.hpp"

#include <algorithm>
#include <random>

#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/views/fanout_view.hpp>

namespace lut_synth::approximate {

VECBEEEstimator::VECBEEEstimator(uint32_t num_patterns, uint32_t seed,
                                 ErrorMetric metric)
    : num_patterns_(num_patterns), seed_(seed), metric_(metric),
      accumulated_error_(0.0), num_bits_(0), ntk_(nullptr) {}

void VECBEEEstimator::initialize(Ntk const& ntk) {
    ntk_ = &ntk;
    accumulated_error_ = 0.0;

    std::mt19937 rng(seed_);
    mockturtle::partial_simulator sim(ntk.num_pis(), num_patterns_, rng());

    tts_ = std::make_unique<mockturtle::unordered_node_map<TT, Ntk>>(ntk);
    mockturtle::simulate_nodes(ntk, *tts_, sim);
    num_bits_ = sim.num_bits();

    compute_output_sensitivities();
}

void VECBEEEstimator::compute_output_sensitivities() {
    output_sensitivities_ =
        std::make_unique<mockturtle::unordered_node_map<TT, Ntk>>(*ntk_);

    ntk_->foreach_node([&](auto n) {
        TT sensitivity(num_bits_);
        (*output_sensitivities_)[n] = sensitivity;
    });

    ntk_->foreach_po([&](auto signal) {
        Ntk::node node = ntk_->get_node(signal);
        TT all_ones(num_bits_);
        for (auto& block : all_ones._bits) {
            block = ~0ULL;
        }
        (*output_sensitivities_)[node] = all_ones;
    });

    mockturtle::fanout_view fov(*ntk_);

    std::vector<node> topo_order;
    ntk_->foreach_node([&](auto n) {
        if (!ntk_->is_constant(n) && !ntk_->is_pi(n)) {
            topo_order.push_back(n);
        }
    });

    for (auto it = topo_order.rbegin(); it != topo_order.rend(); ++it) {
        node n = *it;
        TT& sens = (*output_sensitivities_)[n];

        fov.foreach_fanout(n, [&](auto fo) {
            TT& fo_sens = (*output_sensitivities_)[fo];
            TT bdiff = compute_boolean_difference(fo);

            uint64_t remaining = num_bits_;
            for (size_t i = 0; i < sens._bits.size(); ++i) {
                uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
                uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
                sens._bits[i] |= (fo_sens._bits[i] & bdiff._bits[i]) & mask;
                remaining -= bits_in_chunk;
            }
        });
    }
}

VECBEEEstimator::TT VECBEEEstimator::compute_boolean_difference(node n) const {
    TT result(num_bits_);
    for (auto& block : result._bits) {
        block = ~0ULL;
    }

    ntk_->foreach_fanin(n, [&](auto signal) {
        node fanin = ntk_->get_node(signal);
        TT fanin_tt = (*tts_)[fanin];
        if (ntk_->is_complemented(signal)) {
            uint64_t remaining = num_bits_;
            for (size_t i = 0; i < fanin_tt._bits.size(); ++i) {
                uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
                uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
                fanin_tt._bits[i] = (~fanin_tt._bits[i]) & mask;
                remaining -= bits_in_chunk;
            }
        }

        if (ntk_->is_and(n)) {
            uint64_t remaining = num_bits_;
            for (size_t i = 0; i < result._bits.size(); ++i) {
                uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
                uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
                result._bits[i] &= fanin_tt._bits[i] & mask;
                remaining -= bits_in_chunk;
            }
        }
    });

    return result;
}

double VECBEEEstimator::estimate(LAC const& lac) {
    if (!ntk_ || !tts_ || !output_sensitivities_) return 1.0;

    TT target = (*tts_)[lac.target];
    TT sensitivity = (*output_sensitivities_)[lac.target];

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
        node node1 = ntk_->get_node(lac.divisor1);
        candidate = (*tts_)[node1];
        if (ntk_->is_complemented(lac.divisor1)) {
            uint64_t remaining = num_bits_;
            for (size_t i = 0; i < candidate._bits.size(); ++i) {
                uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
                uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
                candidate._bits[i] = (~candidate._bits[i]) & mask;
                remaining -= bits_in_chunk;
            }
        }
        break;
    }
    case LACType::TwoInput: {
        node node1 = ntk_->get_node(lac.divisor1);
        node node2 = ntk_->get_node(lac.divisor2);
        TT tt1 = (*tts_)[node1];
        TT tt2 = (*tts_)[node2];

        if (ntk_->is_complemented(lac.divisor1)) {
            uint64_t remaining = num_bits_;
            for (size_t i = 0; i < tt1._bits.size(); ++i) {
                uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
                uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
                tt1._bits[i] = (~tt1._bits[i]) & mask;
                remaining -= bits_in_chunk;
            }
        }
        if (ntk_->is_complemented(lac.divisor2)) {
            uint64_t remaining = num_bits_;
            for (size_t i = 0; i < tt2._bits.size(); ++i) {
                uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
                uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
                tt2._bits[i] = (~tt2._bits[i]) & mask;
                remaining -= bits_in_chunk;
            }
        }

        switch (lac.func) {
        case TwoInputFunc::And:
        case TwoInputFunc::Nand:
            for (size_t i = 0; i < candidate._bits.size(); ++i) {
                candidate._bits[i] = tt1._bits[i] & tt2._bits[i];
            }
            break;
        case TwoInputFunc::Or:
        case TwoInputFunc::Nor:
            for (size_t i = 0; i < candidate._bits.size(); ++i) {
                candidate._bits[i] = tt1._bits[i] | tt2._bits[i];
            }
            break;
        case TwoInputFunc::Xor:
        case TwoInputFunc::Xnor:
            for (size_t i = 0; i < candidate._bits.size(); ++i) {
                candidate._bits[i] = tt1._bits[i] ^ tt2._bits[i];
            }
            break;
        }

        if (lac.func == TwoInputFunc::Nand || lac.func == TwoInputFunc::Nor ||
            lac.func == TwoInputFunc::Xnor) {
            uint64_t remaining = num_bits_;
            for (size_t i = 0; i < candidate._bits.size(); ++i) {
                uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
                uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
                candidate._bits[i] = (~candidate._bits[i]) & mask;
                remaining -= bits_in_chunk;
            }
        }
        break;
    }
    }

    TT error_mask(num_bits_);
    uint64_t remaining = num_bits_;
    for (size_t i = 0; i < error_mask._bits.size(); ++i) {
        uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
        uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
        error_mask._bits[i] = ((target._bits[i] ^ candidate._bits[i]) & sensitivity._bits[i]) & mask;
        remaining -= bits_in_chunk;
    }

    return compute_error_from_bdiff(sensitivity, error_mask);
}

double VECBEEEstimator::compute_error_from_bdiff(TT const& /*bdiff*/,
                                                  TT const& error_mask) const {
    uint64_t error_bits = 0;
    uint64_t remaining = num_bits_;

    for (size_t i = 0; i < error_mask._bits.size() && remaining > 0; ++i) {
        uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
        uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
        error_bits += __builtin_popcountll(error_mask._bits[i] & mask);
        remaining -= bits_in_chunk;
    }

    switch (metric_) {
    case ErrorMetric::ER:
        return static_cast<double>(error_bits) / static_cast<double>(num_bits_);
    case ErrorMetric::MHD:
        return static_cast<double>(error_bits);
    case ErrorMetric::MSE:
        return static_cast<double>(error_bits * error_bits) /
               static_cast<double>(num_bits_ * num_bits_);
    default:
        break;
    }
    return 1.0;
}

void VECBEEEstimator::update_after_apply(LAC const& lac) {
    accumulated_error_ += lac.error_delta;
}

double VECBEEEstimator::accumulated_error() const {
    return accumulated_error_;
}

ErrorMetric VECBEEEstimator::metric() const {
    return metric_;
}

VECBEEEstimator::TT const& VECBEEEstimator::get_tt(node n) const {
    return (*tts_)[n];
}

uint64_t VECBEEEstimator::num_bits() const {
    return num_bits_;
}

} // namespace lut_synth::approximate
