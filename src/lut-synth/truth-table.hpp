#pragma once

#include <kitty/dynamic_truth_table.hpp>
#include <string>
#include <vector>

namespace lut_synth {

/*! \brief Container for multi-output Boolean truth tables. */
class TruthTable {
  public:
    TruthTable() = default;

    /*! \brief Construct from complete truth tables. */
    explicit TruthTable(std::vector<kitty::dynamic_truth_table> const &tts)
        : tts_(tts), on_set_(tts), off_set_() {}

    /*! \brief Construct from on-set and off-set (with don't cares). */
    TruthTable(std::vector<kitty::dynamic_truth_table> const &on,
               std::vector<kitty::dynamic_truth_table> const &off)
        : tts_(on), on_set_(on), off_set_(off), has_dont_cares_(true) {}

    /*! \brief Read truth tables from file. */
    void read(const std::string &filename);

    /*! \brief Write truth tables to file. */
    void write(const std::string &filename) const;

    /*! \brief Print statistics to stdout. */
    void print_stats() const;

    std::vector<kitty::dynamic_truth_table> const &get_tts() const { return tts_; }

    /*! \brief Check if truth table has don't-care entries. */
    bool has_dont_cares() const { return has_dont_cares_; }

    /*! \brief Get on-set (1 entries). */
    std::vector<kitty::dynamic_truth_table> const &get_on_set() const { return on_set_; }

    /*! \brief Get off-set (0 entries). */
    std::vector<kitty::dynamic_truth_table> const &get_off_set() const { return off_set_; }

    /*! \brief Create new TruthTable with modified values, preserving don't-cares. */
    TruthTable with_approximated_tts(
        std::vector<kitty::dynamic_truth_table> const& approx_tts) const;

    bool empty() const { return tts_.empty(); }

    /*! \brief Return number of outputs. */
    size_t size() const { return tts_.size(); }

  private:
    std::vector<kitty::dynamic_truth_table> tts_;
    std::vector<kitty::dynamic_truth_table> on_set_;
    std::vector<kitty::dynamic_truth_table> off_set_;
    bool has_dont_cares_ = false;
};

} // namespace lut_synth
