#pragma once

#include <cstdint>

#include <kitty/partial_truth_table.hpp>

namespace lut_synth::approximate::tt_ops {

kitty::partial_truth_table compute_and(kitty::partial_truth_table const& a,
                                       kitty::partial_truth_table const& b,
                                       uint64_t num_bits);

kitty::partial_truth_table compute_xor(kitty::partial_truth_table const& a,
                                       kitty::partial_truth_table const& b,
                                       uint64_t num_bits);

kitty::partial_truth_table compute_not(kitty::partial_truth_table const& a,
                                       uint64_t num_bits);

uint64_t count_ones(kitty::partial_truth_table const& tt, uint64_t num_bits);

uint64_t count_diff_bits(kitty::partial_truth_table const& a,
                         kitty::partial_truth_table const& b,
                         uint64_t num_bits);

} // namespace lut_synth::approximate::tt_ops
