#include "lut-synth/error.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

#include <kitty/bit_operations.hpp>

namespace lut_synth {

std::vector<uint32_t> decode_integers(
    std::vector<kitty::dynamic_truth_table> const& tts) {
    assert(!tts.empty());
    uint32_t num_vars = tts[0].num_vars();
    uint32_t num_inputs = 1u << num_vars;
    uint32_t num_outputs = tts.size();

    std::vector<uint32_t> result(num_inputs, 0u);
    for (uint32_t i = 0; i < num_outputs; ++i) {
        uint32_t bit_weight = 1u << (num_outputs - 1 - i);
        for (uint32_t x = 0; x < num_inputs; ++x) {
            if (kitty::get_bit(tts[i], x)) {
                result[x] += bit_weight;
            }
        }
    }
    return result;
}

IntegerError compute_integer_error(
    std::vector<kitty::dynamic_truth_table> const& exact_tts,
    std::vector<kitty::dynamic_truth_table> const& approx_tts,
    std::vector<double> const& weights) {
    assert(!exact_tts.empty());
    assert(exact_tts.size() == approx_tts.size());

    uint32_t num_vars = exact_tts[0].num_vars();
    uint32_t num_inputs = 1u << num_vars;

    std::vector<uint32_t> exact_ints = decode_integers(exact_tts);
    std::vector<uint32_t> approx_ints = decode_integers(approx_tts);

    bool uniform = weights.empty();
    double w_sum = uniform ? 1.0 : 0.0;
    if (!uniform) {
        assert(weights.size() == num_inputs);
        for (double w : weights) w_sum += w;
    }

    IntegerError err;
    err.num_inputs = num_inputs;
    err.num_correct = 0;
    err.per_input.resize(num_inputs);

    double weighted_sum = 0.0;
    uint32_t worst = 0;
    uint32_t num_wrong = 0;

    for (uint32_t x = 0; x < num_inputs; ++x) {
        int32_t diff = static_cast<int32_t>(approx_ints[x]) -
                       static_cast<int32_t>(exact_ints[x]);
        err.per_input[x] = diff;
        uint32_t abs_diff = static_cast<uint32_t>(std::abs(diff));

        double w = uniform ? (1.0 / num_inputs) : (weights[x] / w_sum);
        weighted_sum += w * abs_diff;
        worst = std::max(worst, abs_diff);
        if (diff != 0) ++num_wrong;
    }

    err.worst_case = worst;
    err.weighted_mean = weighted_sum;
    err.error_rate = static_cast<double>(num_wrong) / num_inputs;
    err.num_correct = num_inputs - num_wrong;

    return err;
}

} // namespace lut_synth
