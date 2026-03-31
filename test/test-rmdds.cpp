#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <lut-synth/synthesis/rmdds.hpp>
#include <lut-synth/synthesis/synthesis.hpp>
#include <lut-synth/resynthesis/resynthesis-util.hpp>
#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>

TEST_CASE("rmdds synthesize and gate", "[rmdds]") {
    kitty::dynamic_truth_table tt(2);
    kitty::create_from_binary_string(tt, "1000");
    std::vector<kitty::dynamic_truth_table> tts = {tt};
    auto xag = lut_synth::synthesize_rmdds(tts);
    CHECK(xag.num_pis() == 2);
    CHECK(xag.num_pos() == 1);
    CHECK(lut_synth::count_ands(xag) == 1);
}

TEST_CASE("rmdds synthesize xor gate", "[rmdds]") {
    kitty::dynamic_truth_table tt(2);
    kitty::create_from_binary_string(tt, "0110");
    std::vector<kitty::dynamic_truth_table> tts = {tt};
    auto xag = lut_synth::synthesize_rmdds(tts);
    CHECK(xag.num_pis() == 2);
    CHECK(xag.num_pos() == 1);
    CHECK(lut_synth::count_ands(xag) == 0);
}

TEST_CASE("rmdds equivalent to ss method", "[rmdds]") {
    kitty::dynamic_truth_table tt(3);
    kitty::create_from_binary_string(tt, "10110100");
    std::vector<kitty::dynamic_truth_table> tts = {tt};
    auto rmdds_xag = lut_synth::synthesize_rmdds(tts);
    auto ss_xag = lut_synth::synthesize_xag_network(tts, "ss");
    auto result = lut_synth::check_equivalence(rmdds_xag, ss_xag);
    CHECK(result.completed);
    CHECK(result.equivalent);
}

TEST_CASE("rmdds dispatcher integration", "[rmdds]") {
    kitty::dynamic_truth_table tt(3);
    kitty::create_from_binary_string(tt, "10110100");
    std::vector<kitty::dynamic_truth_table> tts = {tt};
    auto xag1 = lut_synth::synthesize_xag_network(tts, "rmdds");
    auto xag2 = lut_synth::synthesize_xag_network(tts, "rm");
    CHECK(xag1.num_pis() == 3);
    CHECK(xag2.num_pis() == 3);
    auto result = lut_synth::check_equivalence(xag1, xag2);
    CHECK(result.completed);
    CHECK(result.equivalent);
}
