#include <kitty/constructors.hpp>
#include <kitty/operations.hpp>
#include <mockturtle/algorithms/decomposition.hpp>
#include <mockturtle/networks/xag.hpp>

#include <cassert>
#include <cstdint>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <unordered_set>

namespace lut_synth {

namespace {

class Decomposer {
  public:
    Decomposer(mockturtle::xag_network &xag, uint32_t max_lut) : xag_(xag), max_lut_(max_lut) {}

    mockturtle::xag_network::signal
    decompose(kitty::dynamic_truth_table const &tt,
              std::vector<mockturtle::xag_network::signal> const &pis) {
        if (auto it = cache_.find(tt); it != cache_.end()) {
            return it->second;
        }

        if (auto it = cache_.find(~tt); it != cache_.end()) {
            mockturtle::xag_network::signal neg = xag_.create_not(it->second);
            cache_.insert({tt, neg});
            return neg;
        }

        if (kitty::is_const0(tt)) {
            mockturtle::xag_network::signal sig = xag_.get_constant(false);
            cache_.insert({tt, sig});
            return sig;
        }

        if (kitty::is_const0(~tt)) {
            mockturtle::xag_network::signal sig = xag_.get_constant(true);
            cache_.insert({tt, sig});
            return sig;
        }

        std::vector<uint32_t> support;
        for (uint32_t i = 0u; i < tt.num_vars(); ++i) {
            if (kitty::has_var(tt, i)) {
                support.push_back(i);
            }
        }

        if (support.size() == 1u) {
            kitty::dynamic_truth_table var_tt = tt.construct();
            kitty::create_nth_var(var_tt, support.front());
            const mockturtle::xag_network::signal res =
                (tt == var_tt) ? pis[support.front()] : xag_.create_not(pis[support.front()]);
            cache_.insert({tt, res});
            return res;
        }

        if (support.size() <= max_lut_) {
            mockturtle::xag_network::signal res = synthesize_lut(tt, pis, support);
            cache_.insert({tt, res});
            return res;
        }

        const uint32_t pivot = select_variable(tt, support);
        const kitty::dynamic_truth_table c0 = kitty::cofactor0(tt, pivot);
        const kitty::dynamic_truth_table c1 = kitty::cofactor1(tt, pivot);
        const mockturtle::xag_network::signal f0 = decompose(c0, pis);
        const mockturtle::xag_network::signal f1 = decompose(c1, pis);
        mockturtle::xag_network::signal res = xag_.create_ite(pis[pivot], f1, f0);
        cache_.insert({tt, res});
        return res;
    }

  private:
    mockturtle::xag_network::signal
    synthesize_lut(kitty::dynamic_truth_table const &tt,
                   std::vector<mockturtle::xag_network::signal> const &pis,
                   std::vector<uint32_t> const &support) {
        std::vector<mockturtle::xag_network::signal> projection;
        projection.reserve(support.size());
        for (auto idx : support) {
            projection.push_back(pis[idx]);
        }

        std::vector<uint32_t> local_vars(support.size());
        std::iota(local_vars.begin(), local_vars.end(), 0u);

        kitty::dynamic_truth_table reduced(support.size());
        for (uint32_t i = 0u; i < (1u << support.size()); ++i) {
            uint32_t original_idx = 0u;
            for (uint32_t j = 0u; j < support.size(); ++j) {
                if ((i >> j) & 1u) {
                    original_idx |= (1u << support[j]);
                }
            }
            if (kitty::get_bit(tt, original_idx)) {
                kitty::set_bit(reduced, i);
            }
        }

        return mockturtle::positive_davio_decomposition(xag_, reduced, local_vars, projection);
    }

    uint32_t select_variable(kitty::dynamic_truth_table const &tt,
                             std::vector<uint32_t> const &support) const {
        uint32_t best_var = support.front();
        uint32_t min_m = std::numeric_limits<uint32_t>::max();

        for (auto var : support) {
            const kitty::dynamic_truth_table c0 = kitty::cofactor0(tt, var);
            const kitty::dynamic_truth_table c1 = kitty::cofactor1(tt, var);

            std::unordered_set<uint32_t> columns;
            for (uint32_t i = 0u; i < (1u << (tt.num_vars() - 1)); ++i) {
                const uint32_t column =
                    (kitty::get_bit(c0, i) ? 1u : 0u) | ((kitty::get_bit(c1, i) ? 1u : 0u) << 1u);
                columns.insert(column);
            }

            if (columns.size() < min_m) {
                min_m = static_cast<uint32_t>(columns.size());
                best_var = var;
            }
        }

        return best_var;
    }

    mockturtle::xag_network &xag_;
    uint32_t max_lut_;
    std::unordered_map<kitty::dynamic_truth_table, mockturtle::xag_network::signal,
                       kitty::hash<kitty::dynamic_truth_table>>
        cache_;
};

} // namespace

mockturtle::xag_network synthesize_ac(std::vector<kitty::dynamic_truth_table> const &tts,
                                      uint32_t max_lut_size) {
    assert(!tts.empty());
    assert(max_lut_size >= 2 && max_lut_size <= 6);

    mockturtle::xag_network xag;
    Decomposer decomposer(xag, max_lut_size);

    const uint32_t num_vars = tts.front().num_vars();
    for ([[maybe_unused]] auto const &tt : tts) {
        assert(tt.num_vars() == num_vars);
    }

    std::vector<mockturtle::xag_network::signal> pis(num_vars);
    std::generate(pis.begin(), pis.end(), [&]() { return xag.create_pi(); });

    for (auto const &tt : tts) {
        xag.create_po(decomposer.decompose(tt, pis));
    }

    return xag;
}

} // namespace lut_synth
