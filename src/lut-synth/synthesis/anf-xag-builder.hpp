#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>
#include <mockturtle/networks/xag.hpp>

namespace lut_synth {

/*! \brief Extract nonzero ANF term indices from truth table. */
std::vector<uint32_t> extract_anf_terms(kitty::dynamic_truth_table const& anf);

/*! \brief Reorder ANF terms by nearest-neighbor Hamming distance. */
std::vector<uint32_t> reorder_terms_by_hamming(std::vector<uint32_t> const& terms);

/*! \brief Build AND-product for an ANF term, reusing cached sub-products. */
mockturtle::xag_network::signal build_product_cached(
    mockturtle::xag_network& network,
    std::vector<mockturtle::xag_network::signal> const& inputs,
    uint32_t term,
    std::unordered_map<uint32_t, mockturtle::xag_network::signal>& cache);

/*! \brief Build XAG output as XOR-tree of AND-product terms. */
mockturtle::xag_network::signal build_xag_from_terms(
    mockturtle::xag_network& network,
    std::vector<mockturtle::xag_network::signal> const& inputs,
    std::vector<uint32_t> const& terms,
    std::unordered_map<uint32_t, mockturtle::xag_network::signal>& cache);

} // namespace lut_synth
