#include "lut-synth/approximate/resynthesis/narrow-resub.hpp"
#include "lut-synth/approximate/resynthesis/resubals.hpp"
#include "lut-synth/h4-config.hpp"
#include "lut-synth/resynthesis/resynthesis-util.hpp"
#include "lut-synth/synthesis/ss-synthesizer.hpp"
#include "lut-synth/truth-table.hpp"

#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>
#include <lorina/verilog.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/networks/xag.hpp>

namespace {

struct Args {
    std::string input;
    std::string input_verilog;
    std::string output;
    std::string output_verilog;
    std::string method = "narrow";
    double error_bound = 0.05;
    uint32_t num_patterns = 102400;
    uint32_t num_random_starts = 1;
    uint64_t seed = 0;
    bool disable_dont_care = false;
    bool verbose = false;
    std::vector<uint32_t> lock_indices;
    std::string care_patterns_file;
    bool h4 = false;
    uint32_t rank = 0;
    uint32_t precision = 0;
    uint32_t max_pattern_error = 0;
    std::string estimator = "integer";
};

using lut_synth::ceil_log2;
using lut_synth::compute_h4_locked_bits;

Args parse_args(int argc, char* argv[]) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if ((arg == "--input" || arg == "-i") && i + 1 < argc) {
            args.input = argv[++i];
        } else if (arg == "--input-verilog" && i + 1 < argc) {
            args.input_verilog = argv[++i];
        } else if ((arg == "--output" || arg == "-o") && i + 1 < argc) {
            args.output = argv[++i];
        } else if (arg == "--output-verilog" && i + 1 < argc) {
            args.output_verilog = argv[++i];
        } else if (arg == "--method" && i + 1 < argc) {
            args.method = argv[++i];
        } else if (arg == "--error-bound" && i + 1 < argc) {
            args.error_bound = std::stod(argv[++i]);
        } else if (arg == "--num-patterns" && i + 1 < argc) {
            args.num_patterns = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--num-random-starts" && i + 1 < argc) {
            args.num_random_starts = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--seed" && i + 1 < argc) {
            args.seed = std::stoull(argv[++i]);
        } else if (arg == "--disable-dont-care") {
            args.disable_dont_care = true;
        } else if (arg == "--verbose" || arg == "-v") {
            args.verbose = true;
        } else if (arg == "--lock" && i + 1 < argc) {
            std::istringstream ss(argv[++i]);
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                args.lock_indices.push_back(
                    static_cast<uint32_t>(std::stoul(tok)));
            }
        } else if (arg == "--care-patterns" && i + 1 < argc) {
            args.care_patterns_file = argv[++i];
        } else if (arg == "--h4") {
            args.h4 = true;
        } else if (arg == "--rank" && i + 1 < argc) {
            args.rank = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--precision" && i + 1 < argc) {
            args.precision = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--max-pattern-error" && i + 1 < argc) {
            args.max_pattern_error = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--estimator" && i + 1 < argc) {
            args.estimator = argv[++i];
        }
    }
    return args;
}

void print_usage() {
    std::cerr << "Usage: approx-xag (--input <file.tt> | --input-verilog <file.v>) "
              << "[--output <file.tt>] [--output-verilog <file.v>] "
              << "[--method narrow|resubals] "
              << "[--error-bound F] [--num-patterns N] "
              << "[--num-random-starts N] [--seed S] "
              << "[--disable-dont-care] [--verbose] "
              << "[--lock idx1,idx2,...] "
              << "[--h4 --rank R --precision P]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    Args args = parse_args(argc, argv);
    bool have_tt = !args.input.empty();
    bool have_verilog = !args.input_verilog.empty();
    if (have_tt == have_verilog ||
        (args.method != "narrow" && args.method != "resubals")) {
        print_usage();
        return 1;
    }

    lut_synth::TruthTable tt;
    mockturtle::xag_network xag;
    uint32_t num_inputs = 0;
    uint32_t num_outputs = 0;

    if (have_tt) {
        tt.read(args.input);
        num_inputs = tt.get_tts()[0].num_vars();
        num_outputs = static_cast<uint32_t>(tt.size());
        lut_synth::SSSynthesizer synth(-1, args.num_random_starts, args.seed,
                                        args.disable_dont_care);
        xag = synth.synthesize(tt);
    } else {
        auto rc = lorina::read_verilog(args.input_verilog,
                                        mockturtle::verilog_reader(xag));
        if (rc != lorina::return_code::success) {
            std::cerr << "Error: failed to read " << args.input_verilog << "\n";
            return 1;
        }
        num_inputs = xag.num_pis();
        num_outputs = xag.num_pos();
    }

    std::vector<bool> locked_outputs;
    if (args.h4) {
        if (args.rank < 2 || args.precision < 1) {
            std::cerr << "Error: --h4 requires --rank >= 2 and --precision >= 1\n";
            return 1;
        }
        uint32_t locked_bits = compute_h4_locked_bits(args.rank);
        locked_outputs.assign(num_outputs, false);
        for (uint32_t i = 0; i < locked_bits && i < num_outputs; ++i) {
            locked_outputs[i] = true;
        }
    } else if (!args.lock_indices.empty()) {
        locked_outputs.assign(num_outputs, false);
        for (uint32_t idx : args.lock_indices) {
            if (idx < num_outputs) locked_outputs[idx] = true;
        }
    }

    uint32_t and_before = lut_synth::count_ands(xag);
    uint32_t size_before = xag.num_gates();

    uint32_t and_after = 0;
    uint32_t size_after = 0;
    uint32_t lacs_applied = 0;
    uint32_t and_locked = 0;
    double accumulated_error = 0.0;
    mockturtle::xag_network approx_xag;

    std::vector<uint64_t> care_patterns;
    if (!args.care_patterns_file.empty()) {
        std::ifstream in(args.care_patterns_file);
        if (!in) {
            std::cerr << "Error: cannot open " << args.care_patterns_file
                      << "\n";
            return 1;
        }
        uint64_t v;
        while (in >> v) care_patterns.push_back(v);
    }

    if (args.method == "narrow") {
        lut_synth::approximate::NarrowResubParams params;
        params.error_bound = args.error_bound;
        params.num_patterns = args.num_patterns;
        params.seed = static_cast<uint32_t>(args.seed);
        params.locked_outputs = locked_outputs;
        params.max_integer_error_per_pattern = args.max_pattern_error;
        params.care_patterns = care_patterns;

        lut_synth::approximate::NarrowResubResult result =
            lut_synth::approximate::narrow_and_resub(xag, params);

        approx_xag = result.network;
        and_after = result.stats.and_after;
        size_after = result.stats.final_size;
        lacs_applied = result.stats.lacs_applied;
        accumulated_error = result.stats.accumulated_error;
        and_locked = result.stats.and_locked;
    } else {
        lut_synth::approximate::ResubALSParams params;
        params.error_bound = args.error_bound;
        params.num_patterns = args.num_patterns;
        params.seed = static_cast<uint32_t>(args.seed);
        if (args.estimator == "simulation") {
            params.estimator = lut_synth::approximate::EstimatorType::Simulation;
        } else if (args.estimator == "vecbee") {
            params.estimator = lut_synth::approximate::EstimatorType::VECBEE;
        } else if (args.estimator == "miter") {
            params.estimator = lut_synth::approximate::EstimatorType::Miter;
        } else {
            params.estimator = lut_synth::approximate::EstimatorType::Integer;
        }

        lut_synth::approximate::ResubALSResult result =
            lut_synth::approximate::resubals(xag, params);

        approx_xag = result.network;
        and_after = lut_synth::count_ands(result.network);
        size_after = result.stats.final_size;
        lacs_applied = result.stats.lacs_applied;
        accumulated_error = result.stats.actual_error;
    }

    if (!args.output.empty()) {
        if (!have_tt) {
            std::cerr << "Error: --output (TT) requires --input (TT). "
                      << "Use --output-verilog instead.\n";
            return 1;
        }
        mockturtle::default_simulator<kitty::dynamic_truth_table> sim(num_inputs);
        std::vector<kitty::dynamic_truth_table> approx_tts =
            mockturtle::simulate<kitty::dynamic_truth_table>(approx_xag, sim);
        lut_synth::TruthTable out_tt = tt.with_approximated_tts(approx_tts);
        out_tt.write(args.output);
    }

    if (!args.output_verilog.empty()) {
        mockturtle::write_verilog(approx_xag, args.output_verilog);
    }

    std::cout << "{"
              << "\"method\":\"" << args.method << "\""
              << ",\"num_inputs\":" << num_inputs
              << ",\"num_outputs\":" << num_outputs
              << ",\"error_bound\":" << args.error_bound
              << ",\"size_before\":" << size_before
              << ",\"size_after\":" << size_after
              << ",\"and_before\":" << and_before
              << ",\"and_after\":" << and_after
              << ",\"and_locked\":" << and_locked
              << ",\"lacs_applied\":" << lacs_applied
              << ",\"accumulated_error\":" << accumulated_error
              << "}\n";

    return 0;
}
