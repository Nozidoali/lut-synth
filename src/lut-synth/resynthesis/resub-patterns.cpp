#include "lut-synth/resynthesis/resub-patterns.hpp"

namespace lut_synth {

namespace {

using node = mockturtle::xag_network::node;
using signal = mockturtle::xag_network::signal;
using TT = kitty::dynamic_truth_table;

std::optional<signal> find_wire(ResynContext const& ctx) {
    for (auto d : ctx.divisors) {
        if (!ctx.has_tt(d)) continue;
        if (ctx.tt(d) == ctx.target) return ctx.make_sig(d);
        if (~ctx.tt(d) == ctx.target) return ctx.make_not(d);
    }
    return std::nullopt;
}

std::optional<signal> find_xor2(ResynContext const& ctx) {
    for (size_t i = 0; i < ctx.divisors.size(); ++i) {
        if (!ctx.has_tt(ctx.divisors[i])) continue;
        for (size_t j = i + 1; j < ctx.divisors.size(); ++j) {
            if (!ctx.has_tt(ctx.divisors[j])) continue;
            TT xor_tt = ctx.tt(ctx.divisors[i]) ^ ctx.tt(ctx.divisors[j]);
            if (xor_tt == ctx.target) {
                return ctx.ntk.create_xor(ctx.make_sig(ctx.divisors[i]),
                                          ctx.make_sig(ctx.divisors[j]));
            }
            if (~xor_tt == ctx.target) {
                return ctx.ntk.create_not(
                    ctx.ntk.create_xor(ctx.make_sig(ctx.divisors[i]),
                                       ctx.make_sig(ctx.divisors[j])));
            }
        }
    }
    return std::nullopt;
}

std::optional<signal> find_xor3(ResynContext const& ctx) {
    for (size_t i = 0; i < ctx.divisors.size(); ++i) {
        if (!ctx.has_tt(ctx.divisors[i])) continue;
        for (size_t j = i + 1; j < ctx.divisors.size(); ++j) {
            if (!ctx.has_tt(ctx.divisors[j])) continue;
            for (size_t k = j + 1; k < ctx.divisors.size(); ++k) {
                if (!ctx.has_tt(ctx.divisors[k])) continue;
                TT xor_tt = ctx.tt(ctx.divisors[i]) ^ ctx.tt(ctx.divisors[j]) ^
                            ctx.tt(ctx.divisors[k]);
                if (xor_tt == ctx.target || ~xor_tt == ctx.target) {
                    signal xor1 = ctx.ntk.create_xor(
                        ctx.make_sig(ctx.divisors[i]),
                        ctx.make_sig(ctx.divisors[j]));
                    signal xor2 =
                        ctx.ntk.create_xor(xor1, ctx.make_sig(ctx.divisors[k]));
                    return (xor_tt == ctx.target) ? xor2
                                                  : ctx.ntk.create_not(xor2);
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<signal> find_and2(ResynContext const& ctx) {
    if (ctx.mffc_cost < 2) return std::nullopt;
    for (size_t i = 0; i < ctx.divisors.size(); ++i) {
        if (!ctx.has_tt(ctx.divisors[i])) continue;
        TT const& ti = ctx.tt(ctx.divisors[i]);
        for (size_t j = i + 1; j < ctx.divisors.size(); ++j) {
            if (!ctx.has_tt(ctx.divisors[j])) continue;
            TT const& tj = ctx.tt(ctx.divisors[j]);
            if ((ti & tj) == ctx.target)
                return ctx.ntk.create_and(ctx.make_sig(ctx.divisors[i]),
                                          ctx.make_sig(ctx.divisors[j]));
            if ((ti & ~tj) == ctx.target)
                return ctx.ntk.create_and(ctx.make_sig(ctx.divisors[i]),
                                          ctx.make_not(ctx.divisors[j]));
            if ((~ti & tj) == ctx.target)
                return ctx.ntk.create_and(ctx.make_not(ctx.divisors[i]),
                                          ctx.make_sig(ctx.divisors[j]));
            if ((~ti & ~tj) == ctx.target)
                return ctx.ntk.create_and(ctx.make_not(ctx.divisors[i]),
                                          ctx.make_not(ctx.divisors[j]));
        }
    }
    return std::nullopt;
}

std::optional<signal> find_xor_and(ResynContext const& ctx) {
    if (ctx.mffc_cost < 2) return std::nullopt;
    for (size_t i = 0; i < ctx.divisors.size(); ++i) {
        if (!ctx.has_tt(ctx.divisors[i])) continue;
        for (size_t j = i + 1; j < ctx.divisors.size(); ++j) {
            if (!ctx.has_tt(ctx.divisors[j])) continue;
            TT xor_tt =
                ctx.tt(ctx.divisors[i]) ^ ctx.tt(ctx.divisors[j]);
            signal xor_s = ctx.ntk.create_xor(ctx.make_sig(ctx.divisors[i]),
                                               ctx.make_sig(ctx.divisors[j]));
            for (size_t k = 0; k < ctx.divisors.size(); ++k) {
                if (k == i || k == j || !ctx.has_tt(ctx.divisors[k])) continue;
                TT const& tk = ctx.tt(ctx.divisors[k]);
                if ((xor_tt & tk) == ctx.target)
                    return ctx.ntk.create_and(xor_s,
                                              ctx.make_sig(ctx.divisors[k]));
                if ((xor_tt & ~tk) == ctx.target)
                    return ctx.ntk.create_and(xor_s,
                                              ctx.make_not(ctx.divisors[k]));
                if ((~xor_tt & tk) == ctx.target)
                    return ctx.ntk.create_and(ctx.ntk.create_not(xor_s),
                                              ctx.make_sig(ctx.divisors[k]));
                if ((~xor_tt & ~tk) == ctx.target)
                    return ctx.ntk.create_and(ctx.ntk.create_not(xor_s),
                                              ctx.make_not(ctx.divisors[k]));
            }
        }
    }
    return std::nullopt;
}

std::optional<signal> find_and3(ResynContext const& ctx) {
    if (ctx.mffc_cost < 3) return std::nullopt;
    for (size_t i = 0; i < ctx.divisors.size(); ++i) {
        if (!ctx.has_tt(ctx.divisors[i])) continue;
        TT const& ti = ctx.tt(ctx.divisors[i]);
        for (size_t j = i + 1; j < ctx.divisors.size(); ++j) {
            if (!ctx.has_tt(ctx.divisors[j])) continue;
            TT const& tj = ctx.tt(ctx.divisors[j]);
            TT or_tt = ti | tj;
            TT and_tt = ti & tj;
            for (size_t k = 0; k < ctx.divisors.size(); ++k) {
                if (k == i || k == j || !ctx.has_tt(ctx.divisors[k])) continue;
                TT const& tk = ctx.tt(ctx.divisors[k]);
                if ((or_tt & tk) == ctx.target) {
                    signal or_s = ctx.ntk.create_and(
                        ctx.make_not(ctx.divisors[i]),
                        ctx.make_not(ctx.divisors[j]));
                    return ctx.ntk.create_and(ctx.ntk.create_not(or_s),
                                              ctx.make_sig(ctx.divisors[k]));
                }
                if ((and_tt & tk) == ctx.target) {
                    signal and_s = ctx.ntk.create_and(
                        ctx.make_sig(ctx.divisors[i]),
                        ctx.make_sig(ctx.divisors[j]));
                    return ctx.ntk.create_and(and_s,
                                              ctx.make_sig(ctx.divisors[k]));
                }
            }
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<mockturtle::xag_network::signal> find_replacement(ResynContext const& ctx) {
    if (auto s = find_wire(ctx)) return s;
    if (auto s = find_xor2(ctx)) return s;
    if (auto s = find_xor3(ctx)) return s;
    if (auto s = find_and2(ctx)) return s;
    if (auto s = find_xor_and(ctx)) return s;
    if (auto s = find_and3(ctx)) return s;
    return std::nullopt;
}

} // namespace lut_synth
