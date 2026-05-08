#include "lut-synth/resynthesis/resynthesis-util.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <sstream>
#include <mockturtle/algorithms/balancing.hpp>
#include <mockturtle/algorithms/xag_algebraic_rewriting.hpp>
#include <mockturtle/algorithms/balancing/esop_balancing.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/collapse_mapped.hpp>
#include <mockturtle/algorithms/cut_rewriting.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/klut_to_graph.hpp>
#include <mockturtle/algorithms/lut_mapper.hpp>
#include <mockturtle/algorithms/miter.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_minmc2.hpp>
#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/algorithms/refactoring.hpp>
#include <mockturtle/algorithms/functional_reduction.hpp>
#include <mockturtle/algorithms/xag_optimization.hpp>
#include <mockturtle/algorithms/xag_resub_withDC.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/io/write_bench.hpp>
#include <mockturtle/networks/klut.hpp>
#include <mockturtle/utils/cost_functions.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/mapping_view.hpp>

namespace lut_synth {

uint32_t count_ands(xag_network const &xag) {
    uint32_t count = 0;
    xag.foreach_gate([&](auto n) {
        if (xag.is_and(n)) ++count;
    });
    return count;
}

uint32_t count_xors(xag_network const &xag) {
    uint32_t count = 0;
    xag.foreach_gate([&](auto n) {
        if (xag.is_xor(n)) ++count;
    });
    return count;
}

bool abc_cec(xag_network const &ref, xag_network const &cand) {
    std::string ref_path = "/tmp/resyn_cec_ref.bench";
    std::string cand_path = "/tmp/resyn_cec_cand.bench";
    mockturtle::write_bench(ref, ref_path);
    mockturtle::write_bench(cand, cand_path);
    std::string cmd = "abc -q \"cec -n " + ref_path + " " + cand_path + "\" 2>&1";
    std::array<char, 256> buffer;
    std::string output;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return false;
    while (fgets(buffer.data(), buffer.size(), pipe.get())) {
        output += buffer.data();
    }
    std::istringstream ss(output);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.find("Networks are equivalent") != std::string::npos) {
            return true;
        }
        if (line.find("NOT EQUIVALENT") != std::string::npos) {
            return false;
        }
    }
    return false;
}

bool check_equiv(xag_network const &ref, xag_network const &cand) {
    if (ref.num_pis() != cand.num_pis() || ref.num_pos() != cand.num_pos()) {
        return false;
    }
    if (ref.num_pis() == 0) {
        return true;
    }
    std::optional<xag_network> miter_opt = mockturtle::miter<xag_network>(ref, cand);
    if (!miter_opt) {
        return false;
    }
    mockturtle::equivalence_checking_params ps;
    ps.conflict_limit = 100000;
    std::optional<bool> result = mockturtle::equivalence_checking(*miter_opt, ps);
    if (result.has_value()) {
        return *result;
    }
    return false;
}

xag_network apply_klut_decompress(xag_network const &xag, uint32_t lut_size) {
    mockturtle::lut_map_params mps;
    mps.cut_enumeration_ps.cut_size = lut_size;
    mockturtle::mapping_view<xag_network> mapped{xag};
    mockturtle::lut_map(mapped, mps);
    std::optional<mockturtle::klut_network> klut_opt = mockturtle::collapse_mapped_network<mockturtle::klut_network>(mapped);
    if (!klut_opt) {
        return xag;
    }
    return mockturtle::convert_klut_to_graph<xag_network>(*klut_opt);
}

xag_network apply_cost_generic_resub(xag_network const &xag,
                                      CostGenericResubParams const &ps,
                                      CostGenericResubStats *stats) {
    CostGenericResubResult result = cost_generic_resub_mc(xag, ps);
    if (stats) *stats = result.stats;
    return result.network;
}

xag_network apply_cut_rewriting(xag_network const &xag,
                                 mockturtle::xag_npn_resynthesis<xag_network> &resyn,
                                 mockturtle::cut_rewriting_params const &ps) {
    return mockturtle::cleanup_dangling(
        mockturtle::cut_rewriting<xag_network,
                                  decltype(resyn),
                                  mockturtle::mc_cost<xag_network>>(xag, resyn, ps));
}

xag_network apply_esop_balance(xag_network const &xag) {
    return mockturtle::cleanup_dangling(
        mockturtle::balancing(xag, {mockturtle::esop_rebalancing<xag_network>{}}));
}

xag_network apply_cut_rewriting_minmc(xag_network const &xag, uint32_t cut_size) {
    mockturtle::future::xag_minmc_resynthesis<xag_network> resyn;
    mockturtle::cut_rewriting_params ps;
    ps.cut_enumeration_ps.cut_size = std::min(cut_size, 6u);
    return mockturtle::cleanup_dangling(
        mockturtle::cut_rewriting<xag_network,
                                  decltype(resyn),
                                  mockturtle::mc_cost<xag_network>>(xag, resyn, ps));
}

double elapsed_ms(resynth_clock::time_point start, resynth_clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}


xag_network apply_algebraic_rewriting(xag_network const &xag) {
    xag_network copy = mockturtle::cleanup_dangling(xag);
    mockturtle::depth_view depth_xag{copy};
    mockturtle::xag_algebraic_depth_rewriting_params ps;
    ps.strategy = mockturtle::xag_algebraic_depth_rewriting_params::selective;
    mockturtle::xag_algebraic_depth_rewriting(depth_xag, ps);
    return mockturtle::cleanup_dangling(copy);
}

ResynthesisStepInfo make_step(uint32_t num, std::string const &type,
                                       uint32_t before, uint32_t after, double ms) {
    ResynthesisStepInfo step;
    step.step_number = num;
    step.step_type = type;
    step.and_count_before = before;
    step.and_count_after = after;
    step.time_ms = ms;
    return step;
}

xag_network apply_cut_rewriting_minmc_v3(xag_network const &xag,
                                          uint32_t cut_size,
                                          bool use_dont_cares,
                                          uint32_t cut_limit,
                                          bool allow_zero_gain) {
    mockturtle::future::xag_minmc_resynthesis<xag_network> resyn;
    mockturtle::cut_rewriting_params ps;
    ps.cut_enumeration_ps.cut_size = std::min(cut_size, 6u);
    ps.cut_enumeration_ps.cut_limit = cut_limit;
    ps.use_dont_cares = use_dont_cares;
    ps.allow_zero_gain = allow_zero_gain;
    return mockturtle::cleanup_dangling(
        mockturtle::cut_rewriting<xag_network,
                                  decltype(resyn),
                                  mockturtle::mc_cost<xag_network>>(xag, resyn, ps));
}

xag_network apply_constant_fanin_opt(xag_network const &xag) {
    return mockturtle::cleanup_dangling(mockturtle::xag_constant_fanin_optimization(xag));
}

xag_network apply_dont_cares_opt(xag_network const &xag) {
    return mockturtle::cleanup_dangling(mockturtle::xag_dont_cares_optimization(xag));
}

xag_network apply_resub_minmc_withdc(xag_network const &xag,
                                      ResubMinMcDcParams const &user_ps) {
    xag_network copy = xag;
    mockturtle::fanout_view<xag_network> fanout_xag{copy};
    mockturtle::depth_view<mockturtle::fanout_view<xag_network>> depth_xag{fanout_xag};
    mockturtle::resubstitution_params ps;
    ps.use_dont_cares = true;
    ps.window_size = user_ps.window_size;
    ps.max_pis = user_ps.max_pis;
    ps.max_inserts = user_ps.max_inserts;
    ps.max_divisors = user_ps.max_divisors;
    mockturtle::resubstitution_minmc_withDC(depth_xag, ps);
    return mockturtle::cleanup_dangling(copy);
}

xag_network apply_functional_reduction_pass(xag_network const &xag) {
    xag_network copy = xag;
    mockturtle::functional_reduction_params ps;
    mockturtle::functional_reduction(copy, ps);
    return mockturtle::cleanup_dangling(copy);
}

xag_network apply_refactoring_minmc(xag_network const &xag, bool use_dont_cares) {
    mockturtle::future::xag_minmc_resynthesis<xag_network> resyn;
    mockturtle::refactoring_params ps;
    ps.max_pis = 6;
    ps.use_dont_cares = use_dont_cares;
    xag_network copy = xag;
    mockturtle::refactoring(copy, resyn, ps, nullptr, mockturtle::mc_cost<xag_network>{});
    return mockturtle::cleanup_dangling(copy);
}

void ResynthesisV2State::update_if_better(xag_network cand, uint32_t cand_and) {
    if (cand_and < global_best_and && check_equiv(*original, cand)) {
        global_best = std::move(cand);
        global_best_and = cand_and;
    }
}

} // namespace lut_synth
