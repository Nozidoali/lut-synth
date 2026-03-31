#include "lut-synth/approximate/tt-approximation/exact.hpp"
#include "lut-synth/resynthesis/resynthesis-util.hpp"

#include <cassert>
#include <chrono>

#include <kitty/operations.hpp>
#include <mockturtle/algorithms/exact_mc_synthesis.hpp>
#include <mockturtle/networks/xag.hpp>

namespace lut_synth::approximate {

namespace {

bool is_constant(kitty::dynamic_truth_table const& tt) {
    return kitty::is_const0(tt) || kitty::is_const0(~tt);
}

} // namespace

ExactCostResult compute_exact_cost(
    std::vector<kitty::dynamic_truth_table> const& tts,
    uint32_t conflict_limit) {
    assert(!tts.empty());
    assert(tts.front().num_vars() <= 4);

    ExactCostResult result;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    mockturtle::exact_mc_synthesis_params ps;
    ps.conflict_limit = conflict_limit;

    for (auto const& tt : tts) {
        if (is_constant(tt)) {
            continue;
        }

        mockturtle::xag_network xag = mockturtle::exact_mc_synthesis<mockturtle::xag_network>(tt, ps);
        result.and_count += lut_synth::count_ands(xag);
        result.xor_count += lut_synth::count_xors(xag);
    }

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    result.synthesis_time_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count());
    result.solved = true;

    return result;
}

} // namespace lut_synth::approximate
