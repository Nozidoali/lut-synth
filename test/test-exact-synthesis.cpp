#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <lut-synth/synthesis/synthesis.hpp>
#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <mockturtle/algorithms/simulation.hpp>

TEST_CASE("exact and gate", "[exact-synthesis]") {
    kitty::dynamic_truth_table tt(2);
    kitty::create_from_binary_string(tt, "1000");
    auto xag = lut_synth::synthesize_exact(tt);

    CHECK(xag.num_pis() == 2);
    CHECK(xag.num_pos() == 1);

    mockturtle::default_simulator<kitty::dynamic_truth_table> sim(2);
    auto simulated = mockturtle::simulate<kitty::dynamic_truth_table>(xag, sim);
    CHECK(simulated.size() == 1);
    CHECK(simulated[0] == tt);

    uint32_t and_count = 0;
    xag.foreach_gate([&](auto n) {
        if (xag.is_and(n)) ++and_count;
    });
    CHECK(and_count == 1);
}

TEST_CASE("exact xor gate", "[exact-synthesis]") {
    kitty::dynamic_truth_table tt(2);
    kitty::create_from_binary_string(tt, "0110");
    auto xag = lut_synth::synthesize_exact(tt);

    CHECK(xag.num_pis() == 2);
    CHECK(xag.num_pos() == 1);

    mockturtle::default_simulator<kitty::dynamic_truth_table> sim(2);
    auto simulated = mockturtle::simulate<kitty::dynamic_truth_table>(xag, sim);
    CHECK(simulated.size() == 1);
    CHECK(simulated[0] == tt);

    uint32_t and_count = 0;
    xag.foreach_gate([&](auto n) {
        if (xag.is_and(n)) ++and_count;
    });
    CHECK(and_count == 0);
}

TEST_CASE("exact via method dispatcher", "[exact-synthesis]") {
    kitty::dynamic_truth_table tt(3);
    kitty::create_from_binary_string(tt, "11101000");
    std::vector<kitty::dynamic_truth_table> tts = {tt};
    auto xag = lut_synth::synthesize_xag_network(tts, "exact");

    CHECK(xag.num_pis() == 3);
    CHECK(xag.num_pos() == 1);

    mockturtle::default_simulator<kitty::dynamic_truth_table> sim(3);
    auto simulated = mockturtle::simulate<kitty::dynamic_truth_table>(xag, sim);
    CHECK(simulated.size() == 1);
    CHECK(simulated[0] == tt);
}
