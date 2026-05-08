#pragma once

#include <cstdint>

#include <mockturtle/networks/xag.hpp>

namespace lut_synth {

/*! \brief Parameters for ODC-aware AND rewrite (Liu et al. TCAD 2022 Sec. III-A). */
struct DcAndRewriteParams {
    int32_t odc_levels{-1};       /*!< ODC propagation depth; -1 = until POs */
    uint32_t conflict_limit{1000}; /*!< SAT solver conflict budget per query */
    uint32_t max_clauses{50000};   /*!< CNF clause cap per query */
    bool check_xnor{true};         /*!< Test (0,0) DC: AND -> XNOR */
    bool check_proj{true};         /*!< Test (1,0)/(0,1) DC: AND -> single fanin */
    bool check_const{true};        /*!< Test (1,1) DC: AND -> constant 0 */
};

/*! \brief Statistics from one pass of apply_dc_and_rewrite. */
struct DcAndRewriteStats {
    uint32_t num_const{0};   /*!< AND -> 0 substitutions */
    uint32_t num_proj_a{0};  /*!< AND -> first fanin substitutions */
    uint32_t num_proj_b{0};  /*!< AND -> second fanin substitutions */
    uint32_t num_xnor{0};    /*!< AND -> XNOR substitutions */
    uint32_t num_unknown{0}; /*!< Validator returned nullopt (timeout) */
};

/*! \brief Replace AND gates with cheaper logic when one input pattern is a don't-care.
 *
 *  Implements the SDC&ODC technique of Liu et al. "A Don't-Care-Based Approach to
 *  Reducing the Multiplicative Complexity in Logic Networks" (TCAD 2022). For each
 *  2-input AND gate `t = AND(a, b)`, the four input patterns differ from one of:
 *  XNOR(a,b), b, a, const-0 by exactly one entry. If that entry is a network
 *  don't-care, the replacement is functionally equivalent and saves one AND gate.
 *
 *  The check is formulated as a SAT equivalence query under observability
 *  don't-cares via mockturtle's circuit_validator.
 */
mockturtle::xag_network apply_dc_and_rewrite(mockturtle::xag_network const &xag,
                                              DcAndRewriteParams const &ps = {},
                                              DcAndRewriteStats *stats = nullptr);

}  // namespace lut_synth
