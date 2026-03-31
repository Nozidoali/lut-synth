#pragma once

#include <cstdint>

#include <mockturtle/networks/xag.hpp>

namespace lut_synth::approximate {

/*! \brief Type of local approximate change. */
enum class LACType {
    Const0,   /*!< Replace with constant 0 */
    Const1,   /*!< Replace with constant 1 */
    Single,   /*!< Replace with single divisor */
    TwoInput  /*!< Replace with two-input function */
};

/*! \brief Two-input function type for LAC. */
enum class TwoInputFunc {
    And,   /*!< AND gate */
    Or,    /*!< OR gate */
    Xor,   /*!< XOR gate */
    Nand,  /*!< NAND gate */
    Nor,   /*!< NOR gate */
    Xnor   /*!< XNOR gate */
};

/*! \brief Local approximate change representing a node substitution. */
struct LAC {
    using Ntk = mockturtle::xag_network;
    using node = Ntk::node;
    using signal = Ntk::signal;

    node target;            /*!< Target node to replace */
    LACType type;           /*!< Type of replacement */
    signal divisor1;        /*!< First divisor signal */
    signal divisor2;        /*!< Second divisor signal (for TwoInput) */
    TwoInputFunc func;      /*!< Function type (for TwoInput) */
    int32_t size_gain;      /*!< Size reduction (MFFC size - new nodes) */
    double error_delta;     /*!< Error rate introduced (for compatibility) */
    uint64_t error_count;   /*!< Error count in patterns (integer) */

    LAC();
    LAC(node t, LACType ty, int32_t gain, double err);
    LAC(node t, LACType ty, int32_t gain, uint64_t err_count, uint64_t num_patterns);
    LAC(node t, signal d1, int32_t gain, double err);
    LAC(node t, signal d1, int32_t gain, uint64_t err_count, uint64_t num_patterns);
    LAC(node t, signal d1, signal d2, TwoInputFunc f, int32_t gain, double err);
    LAC(node t, signal d1, signal d2, TwoInputFunc f, int32_t gain,
        uint64_t err_count, uint64_t num_patterns);

    bool is_valid() const;
    double benefit_ratio() const;
    double benefit_ratio_int(uint64_t num_patterns) const;
};

/*! \brief Create replacement signal in network.
 *  \param ntk Network to modify
 *  \param lac LAC to apply
 *  \return Signal representing the replacement
 */
mockturtle::xag_network::signal create_replacement(mockturtle::xag_network& ntk, LAC const& lac);

/*! \brief Compute MFFC size for a node.
 *  \param ntk Network
 *  \param n Node to compute MFFC for
 *  \return MFFC size
 */
uint32_t compute_mffc_size(mockturtle::xag_network const& ntk,
                           mockturtle::xag_network::node n);

} // namespace lut_synth::approximate
