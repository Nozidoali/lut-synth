#include "lut-synth/approximate/tt-approximation/tt-approximation.hpp"
#include "lut-synth/error.hpp"
#include "lut-synth/truth-table.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Args {
    std::string input;
    std::string output;
    double error_bound = 1.0;
    double time_limit = 60.0;
    std::vector<uint32_t> registers;
    std::vector<uint32_t> lock_indices;
    bool verbose = false;
};

Args parse_args(int argc, char* argv[]) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if ((arg == "--input" || arg == "-i") && i + 1 < argc) {
            args.input = argv[++i];
        } else if ((arg == "--output" || arg == "-o") && i + 1 < argc) {
            args.output = argv[++i];
        } else if ((arg == "--error-bound" || arg == "-e") && i + 1 < argc) {
            args.error_bound = std::stod(argv[++i]);
        } else if ((arg == "--time-limit" || arg == "-t") && i + 1 < argc) {
            args.time_limit = std::stod(argv[++i]);
        } else if (arg == "--registers" && i + 1 < argc) {
            std::string val(argv[++i]);
            std::istringstream ss(val);
            std::string token;
            while (std::getline(ss, token, ',')) {
                args.registers.push_back(
                    static_cast<uint32_t>(std::stoul(token)));
            }
        } else if (arg == "--lock" && i + 1 < argc) {
            std::string val(argv[++i]);
            std::istringstream ss2(val);
            std::string token;
            while (std::getline(ss2, token, ',')) {
                args.lock_indices.push_back(
                    static_cast<uint32_t>(std::stoul(token)));
            }
        } else if (arg == "--verbose" || arg == "-v") {
            args.verbose = true;
        }
    }
    return args;
}

void print_usage() {
    std::cerr << "Usage: approx-tt --input <file> --output <file> "
              << "[--error-bound <val>] [--time-limit <sec>] "
              << "[--registers 1,1,6,6,6] [--lock 0,1] [--verbose]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    Args args = parse_args(argc, argv);
    if (args.input.empty() || args.output.empty()) {
        print_usage();
        return 1;
    }

    lut_synth::TruthTable tt;
    tt.read(args.input);

    lut_synth::approximate::TTApproxParams params;
    params.error_bound = args.error_bound;
    params.time_limit = args.time_limit;
    params.verbose = args.verbose;
    params.register_bitsizes = args.registers;
    if (!args.lock_indices.empty()) {
        uint32_t num_outputs = static_cast<uint32_t>(tt.get_tts().size());
        params.locked_outputs.assign(num_outputs, false);
        for (uint32_t idx : args.lock_indices) {
            if (idx < num_outputs) {
                params.locked_outputs[idx] = true;
            }
        }
    }

    lut_synth::approximate::TTApproxResult result =
        lut_synth::approximate::approximate_truth_table_ilp(tt.get_tts(), params);

    if (!result.solved) {
        tt.write(args.output);
        std::cout << "{\"solved\":false}\n";
        return 0;
    }

    lut_synth::TruthTable approx_tt = tt.with_approximated_tts(result.approx_tts);
    approx_tt.write(args.output);

    lut_synth::IntegerError error =
        lut_synth::compute_integer_error(tt.get_tts(), result.approx_tts);

    std::cout << "{"
              << "\"solved\":true"
              << ",\"bits_flipped\":" << result.bits_flipped
              << ",\"worst_case_error\":" << error.worst_case
              << ",\"weighted_mean_error\":" << error.weighted_mean
              << ",\"error_rate\":" << error.error_rate
              << ",\"monomials_before\":" << result.monomials_before
              << ",\"monomials_after\":" << result.monomials_after
              << ",\"ss_and_estimate\":" << result.ss_and_estimate
              << ",\"ss_and_actual\":" << result.ss_and_actual
              << "}\n";

    return 0;
}
