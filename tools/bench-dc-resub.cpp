#include "lut-synth/resynthesis/dc-and-rewrite.hpp"
#include "lut-synth/resynthesis/resynthesis.hpp"
#include "lut-synth/resynthesis/resynthesis-util.hpp"
#include "lut-synth/synthesis/ss-synthesizer.hpp"
#include "lut-synth/truth-table.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include <mockturtle/algorithms/cleanup.hpp>

namespace {

struct Run {
    uint32_t and_count;
    double seconds;
};

Run run_v3(mockturtle::xag_network const &xag,
           lut_synth::ResynthesisV3Params params) {
    using clock = std::chrono::high_resolution_clock;
    clock::time_point t0 = clock::now();
    mockturtle::xag_network out = lut_synth::resynthesize_xag_v3(xag, params);
    out = mockturtle::cleanup_dangling(out);
    double secs = std::chrono::duration<double>(clock::now() - t0).count();
    return {lut_synth::count_ands(out), secs};
}

void run_mode(mockturtle::xag_network const &xag, std::string const &label,
              bool minmc_dc, bool dc_and) {
    lut_synth::ResynthesisV3Params p;
    p.timeout_s = 180.0;
    p.use_minmc_dc_resub = minmc_dc;
    p.use_dc_and_rewrite = dc_and;
    Run r = run_v3(xag, p);
    std::cout << label << ": and=" << r.and_count << " time=" << r.seconds << "s\n";
}

}  // namespace

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: bench-dc-resub <file.tt>\n";
        return 1;
    }
    std::string path = argv[1];

    lut_synth::TruthTable tt;
    tt.read(path);
    lut_synth::SSSynthesizer synth(-1, 1, 0, false);
    mockturtle::xag_network xag = synth.synthesize(tt);
    xag = mockturtle::cleanup_dangling(xag);
    uint32_t initial_and = lut_synth::count_ands(xag);
    std::cout << "file=" << path << " inputs=" << xag.num_pis()
              << " outputs=" << xag.num_pos() << " initial_and=" << initial_and << "\n";

    {
        lut_synth::DcAndRewriteStats raw_stats;
        using clock = std::chrono::high_resolution_clock;
        clock::time_point t0 = clock::now();
        mockturtle::xag_network raw = lut_synth::apply_dc_and_rewrite(xag, {}, &raw_stats);
        double secs = std::chrono::duration<double>(clock::now() - t0).count();
        std::cout << "raw dc_and: " << initial_and << " -> " << lut_synth::count_ands(raw)
                  << " (xnor=" << raw_stats.num_xnor << " proj=" << (raw_stats.num_proj_a + raw_stats.num_proj_b)
                  << " const=" << raw_stats.num_const << " unknown=" << raw_stats.num_unknown
                  << ") time=" << secs << "s\n";
    }
    run_mode(xag, "baseline       (none)", false, false);
    run_mode(xag, "minmc_dc only        ", true,  false);
    run_mode(xag, "dc_and only          ", false, true);
    run_mode(xag, "both                 ", true,  true);

    lut_synth::ResynthesisV3Params p_full;
    p_full.timeout_s = 180.0;
    p_full.use_minmc_dc_resub = true;
    p_full.use_dc_and_rewrite = false;
    mockturtle::xag_network polished = lut_synth::resynthesize_xag_v3(xag, p_full);
    polished = mockturtle::cleanup_dangling(polished);
    uint32_t pre = lut_synth::count_ands(polished);
    lut_synth::DcAndRewriteStats stats;
    using clock = std::chrono::high_resolution_clock;
    clock::time_point t0 = clock::now();
    polished = lut_synth::apply_dc_and_rewrite(polished, {}, &stats);
    double secs = std::chrono::duration<double>(clock::now() - t0).count();
    uint32_t post = lut_synth::count_ands(polished);
    std::cout << "post-polish dc_and: " << pre << " -> " << post
              << " (xnor=" << stats.num_xnor << " proj=" << (stats.num_proj_a + stats.num_proj_b)
              << " const=" << stats.num_const << " unknown=" << stats.num_unknown
              << ") time=" << secs << "s\n";
    return 0;
}
