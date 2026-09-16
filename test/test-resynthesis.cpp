#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <lut-synth/resynthesis/dc-and-rewrite.hpp>
#include <lut-synth/resynthesis/resynthesis.hpp>
#include <lut-synth/resynthesis/resynthesis-util.hpp>
#include <lut-synth/synthesis/synthesis.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/simulation.hpp>

using namespace lut_synth;

TEST_CASE("resynthesis and count monotonic", "[resynthesis]") {
    std::vector<std::pair<int, std::string>> test_cases = {
        {3, "e8"},
        {4, "6996"},
        {4, "8117"},
        {4, "e817"},
        {5, "69969669"},
        {5, "96699669"}
    };

    for (auto const &[n_vars, hex] : test_cases) {
        kitty::dynamic_truth_table tt(n_vars);
        kitty::create_from_hex_string(tt, hex);

        auto xag_orig = synthesize_xag_network({tt}, "ss");
        xag_orig = mockturtle::cleanup_dangling(xag_orig);
        uint32_t orig_ands = count_ands(xag_orig);

        auto xag_resyn = resynthesize_xag_anysyn(xag_orig);
        xag_resyn = mockturtle::cleanup_dangling(xag_resyn);
        uint32_t resyn_ands = count_ands(xag_resyn);

        CHECK(resyn_ands <= orig_ands);

        auto result = check_equivalence(xag_orig, xag_resyn);
        CHECK(result.completed);
        CHECK(result.equivalent);
    }
}

TEST_CASE("resynthesis preserves function", "[resynthesis]") {
    std::vector<std::string> methods = {"ss", "davio", "dsd"};

    for (auto const &method : methods) {
        kitty::dynamic_truth_table tt(4);
        kitty::create_from_hex_string(tt, "6996");

        auto xag_orig = synthesize_xag_network({tt}, method);
        auto xag_resyn = resynthesize_xag_anysyn(xag_orig);

        auto result = check_equivalence(xag_orig, xag_resyn);
        CHECK(result.completed);
        CHECK(result.equivalent);

        auto result_tt = check_equivalence(xag_resyn, {tt});
        CHECK(result_tt.completed);
        CHECK(result_tt.equivalent);
    }
}

TEST_CASE("dc and rewrite triggers on satisfiability dont care", "[resynthesis]") {
    mockturtle::xag_network ntk;
    auto a = ntk.create_pi();
    auto b = ntk.create_pi();
    auto c = ntk.create_and(a, b);
    auto t = ntk.create_and(!c, b);
    ntk.create_po(t);

    REQUIRE(count_ands(ntk) == 2);

    DcAndRewriteStats stats;
    auto out = apply_dc_and_rewrite(ntk, {}, &stats);
    out = mockturtle::cleanup_dangling(out);

    uint32_t total_subs = stats.num_const + stats.num_proj_a + stats.num_proj_b + stats.num_xnor;
    CHECK(total_subs >= 1);
    CHECK(count_ands(out) < 2);

    auto eq = check_equivalence(ntk, out);
    CHECK(eq.completed);
    CHECK(eq.equivalent);
}

TEST_CASE("resynthesis with report", "[resynthesis]") {
    kitty::dynamic_truth_table tt(4);
    kitty::create_from_hex_string(tt, "6996");

    auto xag_orig = synthesize_xag_network({tt}, "ss");
    xag_orig = mockturtle::cleanup_dangling(xag_orig);

    auto result = resynthesize_xag_anysyn_with_report(xag_orig);

    CHECK(result.report_data.final_and_count <= result.report_data.initial_and_count);
    CHECK(result.report_data.num_inputs == xag_orig.num_pis());
    CHECK(result.report_data.num_outputs == xag_orig.num_pos());

    auto equiv_result = check_equivalence(xag_orig, result.xag);
    CHECK(equiv_result.completed);
    CHECK(equiv_result.equivalent);
}
