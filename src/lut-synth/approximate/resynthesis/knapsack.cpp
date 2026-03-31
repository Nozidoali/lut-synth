#include "lut-synth/approximate/resynthesis/knapsack.hpp"

#include <algorithm>
#include <cmath>

namespace lut_synth::approximate {

KnapsackResult knapsack_select(std::vector<LAC> const& lacs, double error_budget,
                               double error_granularity) {
    if (lacs.empty() || error_budget <= 0.0) {
        return {{}, 0, 0.0};
    }

    uint32_t capacity = static_cast<uint32_t>(std::ceil(error_budget / error_granularity));
    uint32_t n = static_cast<uint32_t>(lacs.size());

    std::vector<std::vector<int32_t>> dp(n + 1, std::vector<int32_t>(capacity + 1, 0));

    for (uint32_t i = 1; i <= n; ++i) {
        LAC const& lac = lacs[i - 1];
        uint32_t error_units =
            static_cast<uint32_t>(std::ceil(lac.error_delta / error_granularity));

        for (uint32_t w = 0; w <= capacity; ++w) {
            dp[i][w] = dp[i - 1][w];
            if (error_units <= w && lac.size_gain > 0) {
                int32_t with_item = dp[i - 1][w - error_units] + lac.size_gain;
                if (with_item > dp[i][w]) {
                    dp[i][w] = with_item;
                }
            }
        }
    }

    KnapsackResult result;
    result.total_gain = dp[n][capacity];
    result.total_error = 0.0;

    uint32_t w = capacity;
    for (uint32_t i = n; i > 0 && w > 0; --i) {
        if (dp[i][w] != dp[i - 1][w]) {
            LAC const& lac = lacs[i - 1];
            result.selected.push_back(lac);
            result.total_error += lac.error_delta;
            uint32_t error_units =
                static_cast<uint32_t>(std::ceil(lac.error_delta / error_granularity));
            w -= error_units;
        }
    }

    std::reverse(result.selected.begin(), result.selected.end());
    return result;
}

KnapsackResult greedy_select(std::vector<LAC> const& lacs, double error_budget) {
    if (lacs.empty() || error_budget <= 0.0) {
        return {{}, 0, 0.0};
    }

    std::vector<std::pair<double, size_t>> ratios;
    ratios.reserve(lacs.size());

    for (size_t i = 0; i < lacs.size(); ++i) {
        if (lacs[i].is_valid()) {
            ratios.push_back({lacs[i].benefit_ratio(), i});
        }
    }

    std::sort(ratios.begin(), ratios.end(),
              [](auto const& a, auto const& b) { return a.first > b.first; });

    KnapsackResult result;
    result.total_gain = 0;
    result.total_error = 0.0;

    for (auto const& [ratio, idx] : ratios) {
        LAC const& lac = lacs[idx];
        if (result.total_error + lac.error_delta <= error_budget) {
            result.selected.push_back(lac);
            result.total_gain += lac.size_gain;
            result.total_error += lac.error_delta;
        }
    }

    return result;
}

} // namespace lut_synth::approximate
