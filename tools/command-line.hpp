#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <mockturtle/networks/xag.hpp>

namespace lut_synth { class TruthTable; }

namespace lut_synth::command_line {

/*! \brief Parser for `--flag value` command lines.
 *
 *  Each tool declares the flags it accepts and any other flag is rejected,
 *  so a mistyped option fails loudly instead of running with a default.
 *  A value may begin with `-` so that negative numbers parse correctly;
 *  only a leading `--` marks the next token as a new flag.
 */
class Arguments {
  public:
    /*! \brief Parse argv into flag/value pairs. */
    Arguments(int argc, char *argv[]);

    /*! \brief Whether the flag was given at all. */
    bool has(std::string const &flag) const;

    /*! \brief Return the flag's value, or fallback when absent. */
    std::string text(std::string const &flag, std::string const &fallback = "") const;

    /*! \brief Return the flag's value as an unsigned 32-bit integer. */
    uint32_t number(std::string const &flag, uint32_t fallback) const;

    /*! \brief Return the flag's value as an unsigned 64-bit integer. */
    uint64_t large_number(std::string const &flag, uint64_t fallback) const;

    /*! \brief Return the flag's value as a signed integer. */
    int signed_number(std::string const &flag, int fallback) const;

    /*! \brief Return the flag's value as a double. */
    double real(std::string const &flag, double fallback) const;

    /*! \brief Split the flag's value on commas into unsigned integers. */
    std::vector<uint32_t> number_list(std::string const &flag) const;

    /*! \brief Exit with a message if any flag outside \p accepted was given. */
    void reject_unknown(std::vector<std::string> const &accepted) const;

  private:
    std::vector<std::string> order_;
    std::map<std::string, std::string> values_;
};

/*! \brief Read a truth table file, exiting with a message if unusable. */
TruthTable read_truth_table(std::string const &path);

/*! \brief Read a Verilog netlist into an XAG, exiting on a parse failure. */
mockturtle::xag_network read_verilog_network(std::string const &path);

/*! \brief Write an XAG as Verilog; does nothing when \p path is empty. */
void write_verilog_network(mockturtle::xag_network const &network,
                           std::string const &path);

/*! \brief Collects `"key":value` pairs and prints them as one JSON line.
 *
 *  Every tool reports through this class so that downstream scripts can
 *  parse any of them under the same one-line-of-JSON assumption.
 */
class Report {
  public:
    /*! \brief Append a string field. */
    Report &add(std::string const &key, std::string const &value);

    /*! \brief Append an unsigned integer field. */
    Report &add(std::string const &key, uint32_t value);

    /*! \brief Append a floating-point field. */
    Report &add(std::string const &key, double value);

    /*! \brief Append a boolean field. */
    Report &add(std::string const &key, bool value);

    /*! \brief Append gate and AND counts before and after a rewrite. */
    Report &add_delta(mockturtle::xag_network const &before,
                      mockturtle::xag_network const &after);

    /*! \brief Print the accumulated fields to stdout. */
    void print() const;

  private:
    Report &add_raw(std::string const &key, std::string const &value);

    std::vector<std::string> fields_;
};

} // namespace lut_synth::command_line
