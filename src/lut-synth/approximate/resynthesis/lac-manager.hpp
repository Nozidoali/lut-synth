#pragma once

#include <cstdint>
#include <vector>

#include <kitty/partial_truth_table.hpp>
#include <mockturtle/networks/xag.hpp>

#include "lut-synth/approximate/resynthesis/error-estimator.hpp"
#include "lut-synth/approximate/resynthesis/lac.hpp"

namespace lut_synth::approximate {

/*! \brief Divisor information with precomputed unateness scores. */
struct DivisorInfo {
    using node = mockturtle::xag_network::node;
    using signal = mockturtle::xag_network::signal;

    node div;          /*!< Divisor node */
    signal sig_pos;    /*!< Positive polarity signal */
    signal sig_neg;    /*!< Negative polarity signal */
    uint64_t coverage; /*!< Coverage of ON-set or OFF-set */
    double error;      /*!< Error if used as replacement */
};

/*! \brief Manager for LAC generation using unateness-based filtering. */
class LACManager {
public:
    using Ntk = mockturtle::xag_network;
    using node = Ntk::node;
    using signal = Ntk::signal;
    using TT = kitty::partial_truth_table;

    /*! \brief Construct LAC manager.
     *  \param ntk Network to analyze
     *  \param estimator Error estimator
     *  \param max_divisors Maximum number of divisors
     *  \param max_lac_size Maximum LAC size (0=const, 1=single, 2=two-input)
     */
    LACManager(Ntk const& ntk, ErrorEstimator& estimator,
               uint32_t max_divisors, uint32_t max_lac_size);

    /*! \brief Generate all LACs for a target node.
     *  \param target Target node
     *  \param error_budget Remaining error budget
     *  \return Vector of valid LACs
     */
    std::vector<LAC> generate_lacs(node target, double error_budget);

    /*! \brief Generate best LAC for a target node.
     *  \param target Target node
     *  \param error_budget Remaining error budget
     *  \return Best LAC or invalid LAC if none found
     */
    LAC find_best_lac(node target, double error_budget);

    /*! \brief Get all candidate nodes for approximation.
     *  \return Vector of candidate nodes
     */
    std::vector<node> get_candidates() const;

    /*! \brief Get divisor list.
     *  \return Vector of divisor nodes
     */
    std::vector<node> const& divisors() const;

private:
    void collect_divisors();
    void prepare_target(node target);
    void classify_divisors(double error_budget);

    double compute_error_fast(TT const& candidate) const;

    void find_const_lacs(double error_budget, std::vector<LAC>& lacs);
    void find_single_lacs(double error_budget, std::vector<LAC>& lacs);
    void find_two_input_lacs(double error_budget, std::vector<LAC>& lacs);

    Ntk const& ntk_;
    ErrorEstimator& estimator_;
    uint32_t max_divisors_;
    uint32_t max_lac_size_;

    std::vector<node> divisors_;
    node current_target_;
    int32_t current_mffc_;

    TT target_tt_;
    TT on_set_;
    TT off_set_;
    uint64_t num_on_bits_;
    uint64_t num_off_bits_;
    uint64_t num_bits_;

    std::vector<DivisorInfo> pos_unate_divs_;
    std::vector<DivisorInfo> neg_unate_divs_;
    std::vector<DivisorInfo> binate_divs_;
};

} // namespace lut_synth::approximate
