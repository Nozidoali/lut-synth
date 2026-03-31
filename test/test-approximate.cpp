#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <lut-synth/synthesis/synthesis.hpp>
#include <lut-synth/resynthesis/resynthesis.hpp>
#include <lut-synth/error.hpp>

#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/xag.hpp>

TEST_CASE("error metrics basic", "[approximate-resub]") {
    kitty::partial_truth_table orig(64);
    kitty::partial_truth_table approx(64);

    for (uint64_t i = 0; i < 64; ++i) {
        if (i % 2 == 0) kitty::set_bit(orig, i);
        if (i % 2 == 0) kitty::set_bit(approx, i);
    }

    std::vector<kitty::partial_truth_table> orig_vec{orig};
    std::vector<kitty::partial_truth_table> approx_vec{approx};

    double er = lut_synth::compute_error<lut_synth::ErrorMetric::ER>(orig_vec, approx_vec);
    CHECK(er == 0.0);

    kitty::partial_truth_table approx2(64);
    for (uint64_t i = 0; i < 64; ++i) {
        if (i % 4 == 0) kitty::set_bit(approx2, i);
    }
    std::vector<kitty::partial_truth_table> approx2_vec{approx2};

    double er2 = lut_synth::compute_error<lut_synth::ErrorMetric::ER>(orig_vec, approx2_vec);
    CHECK(er2 > 0.0);
    CHECK(er2 < 1.0);
}

TEST_CASE("approximate resub no change", "[approximate-resub]") {
    mockturtle::xag_network xag;
    auto a = xag.create_pi();
    auto b = xag.create_pi();
    auto f = xag.create_and(a, b);
    xag.create_po(f);

    lut_synth::approximate_resub_params ps;
    ps.error_bound = 0.0;
    ps.num_patterns = 1024;

    auto result = lut_synth::approximate_resubstitution(xag, ps);

    mockturtle::default_simulator<kitty::dynamic_truth_table> sim(2);
    auto orig_tts = mockturtle::simulate<kitty::dynamic_truth_table>(xag, sim);
    auto res_tts = mockturtle::simulate<kitty::dynamic_truth_table>(result, sim);

    CHECK(orig_tts[0] == res_tts[0]);
}

TEST_CASE("error metrics mhd", "[approximate-resub]") {
    kitty::partial_truth_table orig(64);
    kitty::partial_truth_table approx(64);

    for (uint64_t i = 0; i < 64; ++i) {
        kitty::set_bit(orig, i);
    }

    std::vector<kitty::partial_truth_table> orig_vec{orig};
    std::vector<kitty::partial_truth_table> approx_vec{approx};

    double mhd = lut_synth::compute_error<lut_synth::ErrorMetric::MHD>(orig_vec, approx_vec);
    CHECK(mhd == 1.0);

    double nmhd = lut_synth::compute_error<lut_synth::ErrorMetric::NMHD>(orig_vec, approx_vec);
    CHECK(nmhd == 1.0);
}
