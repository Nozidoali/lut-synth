#include "command-line.hpp"

#include "lut-synth/resynthesis/resynthesis-util.hpp"
#include "lut-synth/truth-table.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include <lorina/verilog.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/io/write_verilog.hpp>

namespace lut_synth::command_line {

namespace {

[[noreturn]] void abort_with(std::string const &message) {
    std::cerr << "error: " << message << "\n";
    std::exit(1);
}

bool starts_new_flag(std::string const &token) {
    return token.rfind("--", 0) == 0;
}

} // namespace

Arguments::Arguments(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string const flag(argv[i]);
        if (!starts_new_flag(flag)) {
            abort_with("unexpected argument '" + flag + "'");
        }
        std::string value;
        if (i + 1 < argc && !starts_new_flag(argv[i + 1])) {
            value = argv[++i];
        }
        order_.push_back(flag);
        values_[flag] = value;
    }
}

bool Arguments::has(std::string const &flag) const {
    return values_.count(flag) > 0;
}

std::string Arguments::text(std::string const &flag,
                            std::string const &fallback) const {
    return has(flag) ? values_.at(flag) : fallback;
}

uint32_t Arguments::number(std::string const &flag, uint32_t fallback) const {
    return has(flag) ? static_cast<uint32_t>(std::stoul(values_.at(flag)))
                     : fallback;
}

uint64_t Arguments::large_number(std::string const &flag,
                                 uint64_t fallback) const {
    return has(flag) ? std::stoull(values_.at(flag)) : fallback;
}

int Arguments::signed_number(std::string const &flag, int fallback) const {
    return has(flag) ? std::stoi(values_.at(flag)) : fallback;
}

double Arguments::real(std::string const &flag, double fallback) const {
    return has(flag) ? std::stod(values_.at(flag)) : fallback;
}

std::vector<uint32_t> Arguments::number_list(std::string const &flag) const {
    std::vector<uint32_t> parsed;
    if (!has(flag)) return parsed;
    std::istringstream stream(values_.at(flag));
    std::string token;
    while (std::getline(stream, token, ',')) {
        if (!token.empty()) {
            parsed.push_back(static_cast<uint32_t>(std::stoul(token)));
        }
    }
    return parsed;
}

void Arguments::reject_unknown(std::vector<std::string> const &accepted) const {
    for (std::string const &flag : order_) {
        bool recognized = false;
        for (std::string const &candidate : accepted) {
            if (flag == candidate) {
                recognized = true;
                break;
            }
        }
        if (!recognized) {
            abort_with("unknown option '" + flag + "'");
        }
    }
}

TruthTable read_truth_table(std::string const &path) {
    std::ifstream probe(path);
    if (!probe) abort_with("cannot open " + path);
    probe.close();

    TruthTable table;
    table.read(path);
    if (table.empty()) abort_with(path + " contains no truth tables");
    return table;
}

mockturtle::xag_network read_verilog_network(std::string const &path) {
    mockturtle::xag_network network;
    lorina::return_code code =
        lorina::read_verilog(path, mockturtle::verilog_reader(network));
    if (code != lorina::return_code::success) {
        abort_with("cannot parse " + path + " as Verilog");
    }
    return network;
}

void write_verilog_network(mockturtle::xag_network const &network,
                           std::string const &path) {
    if (path.empty()) return;
    mockturtle::write_verilog(network, path);
}

Report &Report::add_raw(std::string const &key, std::string const &value) {
    fields_.push_back("\"" + key + "\":" + value);
    return *this;
}

Report &Report::add(std::string const &key, std::string const &value) {
    return add_raw(key, "\"" + value + "\"");
}

Report &Report::add(std::string const &key, uint32_t value) {
    return add_raw(key, std::to_string(value));
}

Report &Report::add(std::string const &key, double value) {
    return add_raw(key, std::to_string(value));
}

Report &Report::add(std::string const &key, bool value) {
    return add_raw(key, value ? "true" : "false");
}

Report &Report::add_delta(mockturtle::xag_network const &before,
                          mockturtle::xag_network const &after) {
    return add("size_before", before.num_gates())
        .add("size_after", after.num_gates())
        .add("and_before", count_ands(before))
        .add("and_after", count_ands(after));
}

void Report::print() const {
    std::cout << "{";
    for (size_t i = 0; i < fields_.size(); ++i) {
        if (i > 0) std::cout << ",";
        std::cout << fields_[i];
    }
    std::cout << "}\n";
}

} // namespace lut_synth::command_line
