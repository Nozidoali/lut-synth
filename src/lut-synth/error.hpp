#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

#include <kitty/bit_operations.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/partial_truth_table.hpp>

namespace lut_synth {

/*! \brief Integer error statistics between exact and approximate multi-output functions.
 *
 *  Treats multi-bit outputs as unsigned integers (MSB first, LSB last).
 *  Computes weighted absolute integer error E[|f(x) - f'(x)|].
 */
struct IntegerError {
    uint32_t worst_case{0};             /*!< max_x |f(x) - f'(x)| */
    double weighted_mean{0.0};          /*!< sum w(x)*|f(x)-f'(x)| */
    double error_rate{0.0};             /*!< fraction where f(x) != f'(x) */
    uint32_t num_inputs{0};             /*!< 2^n */
    uint32_t num_correct{0};            /*!< inputs where f(x) == f'(x) */
    std::vector<int32_t> per_input;     /*!< f'(x) - f(x) for each x (signed) */
};

/*! \brief Decode multi-bit truth tables to per-input unsigned integers.
 *
 *  tts[0] = MSB, tts[m-1] = LSB. For input pattern x, the decoded
 *  integer is sum_{i=0}^{m-1} bit(tts[i], x) * 2^{m-1-i}.
 *
 *  \param tts Vector of output truth tables (MSB first)
 *  \return Vector of decoded integers, one per input pattern
 */
std::vector<uint32_t> decode_integers(
    std::vector<kitty::dynamic_truth_table> const& tts);

/*! \brief Compute integer error between exact and approximate truth tables.
 *
 *  \param exact_tts Exact output truth tables (MSB first)
 *  \param approx_tts Approximate output truth tables (MSB first)
 *  \param weights Per-input weights (empty = uniform 1/2^n)
 *  \return Integer error statistics
 */
IntegerError compute_integer_error(
    std::vector<kitty::dynamic_truth_table> const& exact_tts,
    std::vector<kitty::dynamic_truth_table> const& approx_tts,
    std::vector<double> const& weights = {});

/*! \brief Error metrics for approximate computing. */
enum class ErrorMetric {
    ER,      /*!< Error rate (fraction of wrong outputs) */
    MED,     /*!< Mean error distance */
    NMED,    /*!< Normalized mean error distance */
    MSE,     /*!< Mean squared error */
    MHD,     /*!< Mean Hamming distance */
    NMHD,    /*!< Normalized mean Hamming distance */
    INTEGER  /*!< Weighted mean absolute integer error */
};

namespace error_detail {

inline uint64_t popcount(uint64_t x) {
    return __builtin_popcountll(x);
}

inline uint64_t abs_diff(uint64_t a, uint64_t b) {
    return a > b ? a - b : b - a;
}

} // namespace error_detail

/*! \brief Compute error between original and approximate truth tables.
 *  \tparam Metric Error metric to use
 *  \param original Original truth tables
 *  \param approximate Approximated truth tables
 *  \return Error value (interpretation depends on metric)
 */
template <ErrorMetric Metric>
double compute_error(std::vector<kitty::partial_truth_table> const& original,
                     std::vector<kitty::partial_truth_table> const& approximate);

template <>
inline double compute_error<ErrorMetric::ER>(
    std::vector<kitty::partial_truth_table> const& original,
    std::vector<kitty::partial_truth_table> const& approximate) {
    assert(original.size() == approximate.size());
    assert(!original.empty());

    uint64_t num_bits = original[0].num_bits();
    uint64_t error_count = 0;

    for (uint64_t i = 0; i < num_bits; ++i) {
        bool has_error = false;
        for (size_t j = 0; j < original.size(); ++j) {
            if (kitty::get_bit(original[j], i) != kitty::get_bit(approximate[j], i)) {
                has_error = true;
                break;
            }
        }
        if (has_error) ++error_count;
    }

    return static_cast<double>(error_count) / static_cast<double>(num_bits);
}

template <>
inline double compute_error<ErrorMetric::MHD>(
    std::vector<kitty::partial_truth_table> const& original,
    std::vector<kitty::partial_truth_table> const& approximate) {
    assert(original.size() == approximate.size());
    assert(!original.empty());

    uint64_t num_bits = original[0].num_bits();
    uint64_t total_hamming = 0;

    for (size_t j = 0; j < original.size(); ++j) {
        std::vector<uint64_t> const& orig_blocks = original[j]._bits;
        std::vector<uint64_t> const& appr_blocks = approximate[j]._bits;
        for (size_t k = 0; k < orig_blocks.size(); ++k) {
            total_hamming += error_detail::popcount(orig_blocks[k] ^ appr_blocks[k]);
        }
    }

    return static_cast<double>(total_hamming) / static_cast<double>(num_bits);
}

template <>
inline double compute_error<ErrorMetric::NMHD>(
    std::vector<kitty::partial_truth_table> const& original,
    std::vector<kitty::partial_truth_table> const& approximate) {
    double mhd = compute_error<ErrorMetric::MHD>(original, approximate);
    return mhd / static_cast<double>(original.size());
}

template <>
inline double compute_error<ErrorMetric::MED>(
    std::vector<kitty::partial_truth_table> const& original,
    std::vector<kitty::partial_truth_table> const& approximate) {
    assert(original.size() == approximate.size());
    assert(!original.empty());

    uint64_t num_bits = original[0].num_bits();
    uint64_t num_outputs = original.size();
    double total_distance = 0.0;

    for (uint64_t i = 0; i < num_bits; ++i) {
        uint64_t orig_val = 0;
        uint64_t appr_val = 0;
        for (size_t j = 0; j < num_outputs; ++j) {
            if (kitty::get_bit(original[j], i)) orig_val |= (1ULL << j);
            if (kitty::get_bit(approximate[j], i)) appr_val |= (1ULL << j);
        }
        total_distance += static_cast<double>(error_detail::abs_diff(orig_val, appr_val));
    }

    return total_distance / static_cast<double>(num_bits);
}

template <>
inline double compute_error<ErrorMetric::NMED>(
    std::vector<kitty::partial_truth_table> const& original,
    std::vector<kitty::partial_truth_table> const& approximate) {
    double med = compute_error<ErrorMetric::MED>(original, approximate);
    uint64_t max_val = (1ULL << original.size()) - 1;
    return med / static_cast<double>(max_val);
}

template <>
inline double compute_error<ErrorMetric::MSE>(
    std::vector<kitty::partial_truth_table> const& original,
    std::vector<kitty::partial_truth_table> const& approximate) {
    assert(original.size() == approximate.size());
    assert(!original.empty());

    uint64_t num_bits = original[0].num_bits();
    uint64_t num_outputs = original.size();
    double total_squared = 0.0;

    for (uint64_t i = 0; i < num_bits; ++i) {
        uint64_t orig_val = 0;
        uint64_t appr_val = 0;
        for (size_t j = 0; j < num_outputs; ++j) {
            if (kitty::get_bit(original[j], i)) orig_val |= (1ULL << j);
            if (kitty::get_bit(approximate[j], i)) appr_val |= (1ULL << j);
        }
        int64_t diff = static_cast<int64_t>(orig_val) - static_cast<int64_t>(appr_val);
        total_squared += static_cast<double>(diff * diff);
    }

    return total_squared / static_cast<double>(num_bits);
}

/*! \brief Compute error with runtime metric selection. */
inline double compute_error_dispatch(
    ErrorMetric metric,
    std::vector<kitty::partial_truth_table> const& original,
    std::vector<kitty::partial_truth_table> const& approximate) {
    switch (metric) {
    case ErrorMetric::ER:
        return compute_error<ErrorMetric::ER>(original, approximate);
    case ErrorMetric::MED:
        return compute_error<ErrorMetric::MED>(original, approximate);
    case ErrorMetric::NMED:
        return compute_error<ErrorMetric::NMED>(original, approximate);
    case ErrorMetric::MSE:
        return compute_error<ErrorMetric::MSE>(original, approximate);
    case ErrorMetric::MHD:
        return compute_error<ErrorMetric::MHD>(original, approximate);
    case ErrorMetric::NMHD:
        return compute_error<ErrorMetric::NMHD>(original, approximate);
    case ErrorMetric::INTEGER:
        return 0.0; // INTEGER metric requires specialized estimator
    }
    return 0.0;
}

} // namespace lut_synth
