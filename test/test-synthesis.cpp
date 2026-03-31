#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <lut-synth/synthesis/synthesis.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/constructors.hpp>

TEST_CASE("synthesize xag and", "[synthesis]") {
    kitty::dynamic_truth_table tt(2);
    kitty::create_from_binary_string(tt, "1000");
    std::vector<kitty::dynamic_truth_table> tts = {tt};
    auto xag = lut_synth::synthesize_xag_network(tts, "ss");
    CHECK(xag.num_pis() == 2);
    CHECK(xag.num_pos() == 1);
}

TEST_CASE("synthesize xag xor", "[synthesis]") {
    kitty::dynamic_truth_table tt(2);
    kitty::create_from_binary_string(tt, "0110");
    std::vector<kitty::dynamic_truth_table> tts = {tt};
    auto xag = lut_synth::synthesize_xag_network(tts, "ss");
    CHECK(xag.num_pis() == 2);
    CHECK(xag.num_pos() == 1);
}
