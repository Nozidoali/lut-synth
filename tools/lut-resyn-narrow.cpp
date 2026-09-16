#include "command-line.hpp"
#include "lut-synth/approximate/resynthesis/narrow-resub.hpp"

#include <fstream>
#include <iostream>

namespace {

void print_usage() {
    std::cerr <<
        "Usage: lut-resyn-narrow --input <file.v> [--output <file.v>] [options]\n"
        "\n"
        "Approximate. For every AND gate, try the five local substitutions that\n"
        "use only its own fanins -- constant 0 or 1, either fanin, or their XOR --\n"
        "and greedily apply the best one that still fits the error budget.\n"
        "Solver-free, and targets AND count specifically.\n"
        "\n"
        "Options:\n"
        "  --input <file.v>           Verilog netlist to approximate (required)\n"
        "  --output <file.v>          Write the approximated netlist\n"
        "  --error-bound <F>          Maximum accumulated error (default: 0.05)\n"
        "  --num-patterns <N>         Simulation patterns (default: 102400)\n"
        "  --seed <N>                 Simulation seed\n"
        "  --max-iterations <N>       Substitution rounds, 0 means unlimited\n"
        "  --max-pattern-error <N>    Reject any substitution whose worst single\n"
        "                             pattern exceeds this integer error\n"
        "  --lock <a,b,...>           Output indices to reproduce exactly\n"
        "  --care-patterns <file>     Treat only these PI assignments as care\n";
}

std::vector<uint64_t> read_care_patterns(std::string const &path) {
    std::vector<uint64_t> patterns;
    if (path.empty()) return patterns;
    std::ifstream input(path);
    if (!input) {
        std::cerr << "error: cannot open " << path << "\n";
        std::exit(1);
    }
    uint64_t pattern = 0;
    while (input >> pattern) patterns.push_back(pattern);
    return patterns;
}

} // namespace

int main(int argc, char *argv[]) {
    lut_synth::command_line::Arguments const arguments(argc, argv);
    arguments.reject_unknown({"--input", "--output", "--error-bound",
                              "--num-patterns", "--seed", "--max-iterations",
                              "--max-pattern-error", "--lock", "--care-patterns",
                              "--help"});
    if (arguments.has("--help") || !arguments.has("--input")) {
        print_usage();
        return arguments.has("--help") ? 0 : 1;
    }

    mockturtle::xag_network const network =
        lut_synth::command_line::read_verilog_network(arguments.text("--input"));

    lut_synth::approximate::NarrowResubParams parameters;
    parameters.error_bound = arguments.real("--error-bound", 0.05);
    parameters.num_patterns = arguments.number("--num-patterns", 102400);
    parameters.seed = arguments.number("--seed", 0);
    parameters.max_iterations = arguments.number("--max-iterations", 0);
    parameters.max_integer_error_per_pattern =
        arguments.number("--max-pattern-error", 0);
    parameters.care_patterns = read_care_patterns(arguments.text("--care-patterns"));

    std::vector<uint32_t> const locked = arguments.number_list("--lock");
    if (!locked.empty()) {
        parameters.locked_outputs.assign(network.num_pos(), false);
        for (uint32_t index : locked) {
            if (index < network.num_pos()) parameters.locked_outputs[index] = true;
        }
    }

    lut_synth::approximate::NarrowResubResult const result =
        lut_synth::approximate::narrow_and_resub(network, parameters);
    lut_synth::command_line::write_verilog_network(result.network,
                                                   arguments.text("--output"));

    lut_synth::command_line::Report()
        .add("method", std::string("narrow"))
        .add("num_inputs", network.num_pis())
        .add("num_outputs", network.num_pos())
        .add("error_bound", parameters.error_bound)
        .add_delta(network, result.network)
        .add("and_locked", result.stats.and_locked)
        .add("lacs_applied", result.stats.lacs_applied)
        .add("lacs_rejected_by_cap", result.stats.lacs_rejected_by_cap)
        .add("accumulated_error", result.stats.accumulated_error)
        .print();
    return 0;
}
