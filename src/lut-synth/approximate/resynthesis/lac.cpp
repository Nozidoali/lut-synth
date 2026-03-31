#include "lut-synth/approximate/resynthesis/lac.hpp"

#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/views/mffc_view.hpp>

namespace lut_synth::approximate {

LAC::LAC()
    : target(0), type(LACType::Const0), divisor1(0), divisor2(0),
      func(TwoInputFunc::And), size_gain(0), error_delta(1.0), error_count(0) {}

LAC::LAC(node t, LACType ty, int32_t gain, double err)
    : target(t), type(ty), divisor1(0), divisor2(0),
      func(TwoInputFunc::And), size_gain(gain), error_delta(err), error_count(0) {}

LAC::LAC(node t, LACType ty, int32_t gain, uint64_t err_count, uint64_t num_patterns)
    : target(t), type(ty), divisor1(0), divisor2(0),
      func(TwoInputFunc::And), size_gain(gain),
      error_delta(num_patterns > 0 ? static_cast<double>(err_count) / num_patterns : 1.0),
      error_count(err_count) {}

LAC::LAC(node t, signal d1, int32_t gain, double err)
    : target(t), type(LACType::Single), divisor1(d1), divisor2(0),
      func(TwoInputFunc::And), size_gain(gain), error_delta(err), error_count(0) {}

LAC::LAC(node t, signal d1, int32_t gain, uint64_t err_count, uint64_t num_patterns)
    : target(t), type(LACType::Single), divisor1(d1), divisor2(0),
      func(TwoInputFunc::And), size_gain(gain),
      error_delta(num_patterns > 0 ? static_cast<double>(err_count) / num_patterns : 1.0),
      error_count(err_count) {}

LAC::LAC(node t, signal d1, signal d2, TwoInputFunc f, int32_t gain, double err)
    : target(t), type(LACType::TwoInput), divisor1(d1), divisor2(d2),
      func(f), size_gain(gain), error_delta(err), error_count(0) {}

LAC::LAC(node t, signal d1, signal d2, TwoInputFunc f, int32_t gain,
         uint64_t err_count, uint64_t num_patterns)
    : target(t), type(LACType::TwoInput), divisor1(d1), divisor2(d2),
      func(f), size_gain(gain),
      error_delta(num_patterns > 0 ? static_cast<double>(err_count) / num_patterns : 1.0),
      error_count(err_count) {}

bool LAC::is_valid() const {
    return size_gain > 0 && error_delta < 1.0;
}

double LAC::benefit_ratio() const {
    if (error_delta <= 0.0) return 0.0;
    return static_cast<double>(size_gain) / error_delta;
}

double LAC::benefit_ratio_int(uint64_t num_patterns) const {
    if (error_count == 0) return 0.0;
    return static_cast<double>(size_gain) * num_patterns / error_count;
}

mockturtle::xag_network::signal create_replacement(mockturtle::xag_network& ntk,
                                                    LAC const& lac) {
    using signal = mockturtle::xag_network::signal;

    switch (lac.type) {
    case LACType::Const0:
        return ntk.get_constant(false);

    case LACType::Const1:
        return ntk.get_constant(true);

    case LACType::Single:
        return lac.divisor1;

    case LACType::TwoInput: {
        signal s1 = lac.divisor1;
        signal s2 = lac.divisor2;

        switch (lac.func) {
        case TwoInputFunc::And:
            return ntk.create_and(s1, s2);
        case TwoInputFunc::Or:
            return ntk.create_or(s1, s2);
        case TwoInputFunc::Xor:
            return ntk.create_xor(s1, s2);
        case TwoInputFunc::Nand:
            return ntk.create_nand(s1, s2);
        case TwoInputFunc::Nor:
            return ntk.create_nor(s1, s2);
        case TwoInputFunc::Xnor:
            return !ntk.create_xor(s1, s2);
        }
    }
    }
    return ntk.get_constant(false);
}

uint32_t compute_mffc_size(mockturtle::xag_network const& ntk,
                           mockturtle::xag_network::node n) {
    mockturtle::fanout_view fov(ntk);
    mockturtle::mffc_view mffc(fov, n);
    return mffc.num_gates();
}

} // namespace lut_synth::approximate
