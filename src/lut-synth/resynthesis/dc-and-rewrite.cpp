#include "lut-synth/resynthesis/dc-and-rewrite.hpp"

#include <bill/sat/interface/abc_bsat2.hpp>
#include <mockturtle/algorithms/circuit_validator.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/utils/index_list/index_list.hpp>
#include <mockturtle/views/fanout_view.hpp>

namespace lut_synth {

namespace {

using fanout_xag = mockturtle::fanout_view<mockturtle::xag_network>;
using validator_t = mockturtle::circuit_validator<fanout_xag,
                                                   bill::solvers::bsat2,
                                                   false, false, true>;
using index_list_t = mockturtle::xag_index_list<false>;
using node = mockturtle::xag_network::node;
using signal = mockturtle::xag_network::signal;

uint32_t literal_for_pi(uint32_t pi_index, bool complemented) {
    return (pi_index << 1) | (complemented ? 1u : 0u);
}

enum class TryResult { Equivalent, NotEquivalent, Unknown };

TryResult try_replace_const0(fanout_xag &fv, validator_t &val, node t,
                              std::vector<node> const &divs) {
    index_list_t list(2);
    list.add_output(0);
    std::optional<bool> res = val.validate(t, divs, list);
    if (!res) return TryResult::Unknown;
    if (!*res) return TryResult::NotEquivalent;
    fv.substitute_node(t, fv.get_constant(false));
    return TryResult::Equivalent;
}

TryResult try_replace_with_signal(fanout_xag &fv, validator_t &val, node t,
                                   std::vector<node> const &divs, uint32_t out_lit,
                                   signal const &replacement) {
    index_list_t list(2);
    list.add_output(out_lit);
    std::optional<bool> res = val.validate(t, divs, list);
    if (!res) return TryResult::Unknown;
    if (!*res) return TryResult::NotEquivalent;
    fv.substitute_node(t, replacement);
    return TryResult::Equivalent;
}

TryResult try_replace_xnor(fanout_xag &fv, validator_t &val, node t,
                            std::vector<node> const &divs, uint32_t lit_a,
                            uint32_t lit_b, signal const &fa, signal const &fb) {
    index_list_t list(2);
    uint32_t xor_lit = list.add_xor(lit_a, lit_b);
    list.add_output(xor_lit ^ 1u);
    std::optional<bool> res = val.validate(t, divs, list);
    if (!res) return TryResult::Unknown;
    if (!*res) return TryResult::NotEquivalent;
    signal xnor = !fv.create_xor(fa, fb);
    fv.substitute_node(t, xnor);
    return TryResult::Equivalent;
}

}  // namespace

mockturtle::xag_network apply_dc_and_rewrite(mockturtle::xag_network const &xag,
                                              DcAndRewriteParams const &ps,
                                              DcAndRewriteStats *stats) {
    mockturtle::xag_network ntk = xag.clone();
    fanout_xag fv{ntk};

    mockturtle::validator_params vps;
    vps.odc_levels = ps.odc_levels;
    vps.conflict_limit = ps.conflict_limit;
    vps.max_clauses = ps.max_clauses;

    validator_t validator(fv, vps);
    DcAndRewriteStats local;

    std::vector<node> ands;
    fv.foreach_gate([&](auto n) {
        if (fv.is_and(n)) ands.push_back(n);
    });

    for (node t : ands) {
        if (fv.is_dead(t)) continue;

        signal fa, fb;
        uint32_t idx = 0;
        fv.foreach_fanin(t, [&](auto f, auto i) {
            if (i == 0) fa = f;
            else fb = f;
            ++idx;
        });
        if (idx != 2) continue;

        node na = fv.get_node(fa);
        node nb = fv.get_node(fb);
        if (na == nb) continue;
        if (fv.is_constant(na) || fv.is_constant(nb)) continue;

        std::vector<node> divs{na, nb};
        uint32_t lit_a = literal_for_pi(1, fv.is_complemented(fa));
        uint32_t lit_b = literal_for_pi(2, fv.is_complemented(fb));

        TryResult r;
        if (ps.check_const) {
            r = try_replace_const0(fv, validator, t, divs);
            if (r == TryResult::Equivalent) { ++local.num_const; continue; }
            if (r == TryResult::Unknown) { ++local.num_unknown; continue; }
        }
        if (ps.check_proj) {
            r = try_replace_with_signal(fv, validator, t, divs, lit_b, fb);
            if (r == TryResult::Equivalent) { ++local.num_proj_b; continue; }
            if (r == TryResult::Unknown) { ++local.num_unknown; continue; }
            r = try_replace_with_signal(fv, validator, t, divs, lit_a, fa);
            if (r == TryResult::Equivalent) { ++local.num_proj_a; continue; }
            if (r == TryResult::Unknown) { ++local.num_unknown; continue; }
        }
        if (ps.check_xnor) {
            r = try_replace_xnor(fv, validator, t, divs, lit_a, lit_b, fa, fb);
            if (r == TryResult::Equivalent) { ++local.num_xnor; continue; }
            if (r == TryResult::Unknown) { ++local.num_unknown; continue; }
        }
    }

    if (stats) *stats = local;
    uint32_t changes = local.num_const + local.num_proj_a + local.num_proj_b + local.num_xnor;
    if (changes == 0) return xag;
    return mockturtle::cleanup_dangling(ntk);
}

}  // namespace lut_synth
