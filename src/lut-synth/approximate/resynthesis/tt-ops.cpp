#include "lut-synth/approximate/resynthesis/tt-ops.hpp"

#include <algorithm>

namespace lut_synth::approximate::tt_ops {

kitty::partial_truth_table compute_and(kitty::partial_truth_table const& a,
                                       kitty::partial_truth_table const& b,
                                       uint64_t num_bits) {
    kitty::partial_truth_table result;
    result._bits.resize(a._bits.size());
    uint64_t remaining = num_bits;
    for (size_t i = 0; i < a._bits.size(); ++i) {
        uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
        uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
        result._bits[i] = (a._bits[i] & b._bits[i]) & mask;
        remaining -= bits_in_chunk;
    }
    return result;
}

kitty::partial_truth_table compute_xor(kitty::partial_truth_table const& a,
                                       kitty::partial_truth_table const& b,
                                       uint64_t num_bits) {
    kitty::partial_truth_table result;
    result._bits.resize(a._bits.size());
    uint64_t remaining = num_bits;
    for (size_t i = 0; i < a._bits.size(); ++i) {
        uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
        uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
        result._bits[i] = (a._bits[i] ^ b._bits[i]) & mask;
        remaining -= bits_in_chunk;
    }
    return result;
}

kitty::partial_truth_table compute_not(kitty::partial_truth_table const& a,
                                       uint64_t num_bits) {
    kitty::partial_truth_table result;
    result._bits.resize(a._bits.size());
    uint64_t remaining = num_bits;
    for (size_t i = 0; i < a._bits.size(); ++i) {
        uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
        uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
        result._bits[i] = (~a._bits[i]) & mask;
        remaining -= bits_in_chunk;
    }
    return result;
}

uint64_t count_ones(kitty::partial_truth_table const& tt, uint64_t num_bits) {
    uint64_t count = 0;
    uint64_t remaining = num_bits;
    for (size_t i = 0; i < tt._bits.size() && remaining > 0; ++i) {
        uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
        uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
        count += __builtin_popcountll(tt._bits[i] & mask);
        remaining -= bits_in_chunk;
    }
    return count;
}

uint64_t count_diff_bits(kitty::partial_truth_table const& a,
                         kitty::partial_truth_table const& b,
                         uint64_t num_bits) {
    uint64_t diff = 0;
    uint64_t remaining = num_bits;
    for (size_t i = 0; i < a._bits.size() && remaining > 0; ++i) {
        uint64_t bits_in_chunk = std::min<uint64_t>(remaining, 64);
        uint64_t mask = bits_in_chunk == 64 ? ~0ULL : ((1ULL << bits_in_chunk) - 1);
        diff += __builtin_popcountll((a._bits[i] ^ b._bits[i]) & mask);
        remaining -= bits_in_chunk;
    }
    return diff;
}

} // namespace lut_synth::approximate::tt_ops
