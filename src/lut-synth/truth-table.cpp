#include "lut-synth/truth-table.hpp"
#include "lut-synth/string-util.hpp"

#include <fstream>
#include <iostream>
#include <kitty/bit_operations.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <stdexcept>

namespace lut_synth {

static uint32_t determine_num_vars(std::size_t num_bits) {
    if (num_bits == 0u) {
        throw std::invalid_argument("truth table string is empty");
    }

    uint32_t num_vars = 0u;
    std::size_t value = num_bits;
    while (value > 1u) {
        if (value % 2u != 0u) {
            throw std::invalid_argument("truth table length must be a power of two");
        }
        value /= 2u;
        ++num_vars;
    }
    return num_vars;
}

void TruthTable::read(const std::string &filename) {
    std::ifstream in(filename);
    if (!in.is_open()) {
        throw std::runtime_error("failed to open truth table file: " + filename);
    }

    std::vector<std::string> rows;
    std::string line;
    while (std::getline(in, line)) {
        lut_synth::util::trim_inplace(line);
        if (line.empty()) {
            continue;
        }
        rows.push_back(line);
    }

    if (rows.empty()) {
        throw std::runtime_error("truth table file is empty: " + filename);
    }

    const size_t num_bits = rows.front().size();
    const uint32_t num_vars = determine_num_vars(num_bits);

    tts_.clear();
    tts_.reserve(rows.size());
    on_set_.clear();
    on_set_.reserve(rows.size());
    off_set_.clear();
    off_set_.reserve(rows.size());
    has_dont_cares_ = false;

    for (auto const &row : rows) {
        if (row.size() != num_bits) {
            throw std::runtime_error("inconsistent truth table length in file: " + filename);
        }
        kitty::dynamic_truth_table tt(num_vars);
        kitty::dynamic_truth_table on(num_vars);
        kitty::dynamic_truth_table off(num_vars);
        for (uint64_t i = 0; i < row.size(); ++i) {
            const char c = row[i];
            if (c == '1') {
                kitty::set_bit(tt, i);
                kitty::set_bit(on, i);
            } else if (c == '0') {
                kitty::set_bit(off, i);
            } else if (c == 'X' || c == 'x') {
                has_dont_cares_ = true;
            } else {
                throw std::runtime_error("invalid truth table character in file: " + filename);
            }
        }
        tts_.push_back(tt);
        on_set_.push_back(on);
        off_set_.push_back(off);
    }
}

void TruthTable::write(const std::string &filename) const {
    if (tts_.empty()) {
        throw std::runtime_error("cannot write empty truth table");
    }

    std::ofstream out(filename);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open file for writing: " + filename);
    }

    for (size_t idx = 0; idx < tts_.size(); ++idx) {
        uint64_t num_bits = 1ULL << tts_[idx].num_vars();
        std::string row;
        row.reserve(num_bits);
        for (uint64_t i = 0; i < num_bits; ++i) {
            if (has_dont_cares_ &&
                !kitty::get_bit(on_set_[idx], i) &&
                !kitty::get_bit(off_set_[idx], i)) {
                row += 'X';
            } else {
                row += kitty::get_bit(tts_[idx], i) ? '1' : '0';
            }
        }
        out << row << "\n";
    }
}

TruthTable TruthTable::with_approximated_tts(
    std::vector<kitty::dynamic_truth_table> const& approx_tts) const {
    assert(approx_tts.size() == tts_.size());

    if (!has_dont_cares_) {
        return TruthTable(approx_tts);
    }

    TruthTable result;
    result.tts_ = approx_tts;
    result.has_dont_cares_ = true;
    result.on_set_.reserve(approx_tts.size());
    result.off_set_.reserve(approx_tts.size());

    for (size_t idx = 0; idx < approx_tts.size(); ++idx) {
        uint64_t num_bits = 1ULL << approx_tts[idx].num_vars();
        kitty::dynamic_truth_table on(approx_tts[idx].num_vars());
        kitty::dynamic_truth_table off(approx_tts[idx].num_vars());
        for (uint64_t i = 0; i < num_bits; ++i) {
            bool is_care = kitty::get_bit(on_set_[idx], i) ||
                           kitty::get_bit(off_set_[idx], i);
            if (!is_care) {
                continue;
            }
            if (kitty::get_bit(approx_tts[idx], i)) {
                kitty::set_bit(on, i);
            } else {
                kitty::set_bit(off, i);
            }
        }
        result.on_set_.push_back(on);
        result.off_set_.push_back(off);
    }

    return result;
}

void TruthTable::print_stats() const {
    if (tts_.empty()) {
        std::cout << "Truth table: empty" << std::endl;
        return;
    }

    const uint32_t num_vars = tts_.front().num_vars();
    const size_t num_outputs = tts_.size();

    std::cout << "Truth table statistics:" << std::endl;
    std::cout << "  Number of inputs: " << num_vars << std::endl;
    std::cout << "  Number of outputs: " << num_outputs << std::endl;
}

} // namespace lut_synth
