#include "command-line.hpp"
#include "lut-synth/approximate/resynthesis/resubals.hpp"

#include <iostream>

namespace {

void print_usage() {
    std::cerr <<
        "Usage: lut-resyn-resubals --input <file.v> [--output <file.v>] [options]\n"
        "\n"
        "Approximate. Three-phase approximate resubstitution: a knapsack picks a\n"
        "batch of local approximate changes, then single changes are applied until\n"
        "the error budget runs out, then dangling logic is swept away. Unlike the\n"
        "narrow pass this enumerates a divisor window, so it is slower but finds\n"
        "substitutions that local fanins alone cannot express.\n"
        "\n"
        "Options:\n"
        "  --input <file.v>      Verilog netlist to approximate (required)\n"
        "  --output <file.v>     Write the approximated netlist\n"
        "  --error-bound <F>     Maximum error (default: 0.05)\n"
        "  --estimator <name>    integer, simulation, vecbee or miter\n"
        "                        (default: integer)\n"
        "  --num-patterns <N>    Simulation patterns (default: 102400)\n"
        "  --max-divisors <N>    Divisor window size (default: 150)\n"
        "  --max-lac-size <N>    Largest local change to consider (default: 2)\n"
        "  --no-knapsack         Skip the phase-one knapsack selection\n"
        "  --seed <N>            Simulation seed\n";
}

lut_synth::approximate::EstimatorType parse_estimator(std::string const &name) {
    if (name == "simulation") return lut_synth::approximate::EstimatorType::Simulation;
    if (name == "vecbee") return lut_synth::approximate::EstimatorType::VECBEE;
    if (name == "miter") return lut_synth::approximate::EstimatorType::Miter;
    if (name == "integer") return lut_synth::approximate::EstimatorType::Integer;
    std::cerr << "error: unknown estimator '" << name
              << "', expected integer, simulation, vecbee or miter\n";
    std::exit(1);
}

} // namespace

int main(int argc, char *argv[]) {
    lut_synth::command_line::Arguments const arguments(argc, argv);
    arguments.reject_unknown({"--input", "--output", "--error-bound", "--estimator",
                              "--num-patterns", "--max-divisors", "--max-lac-size",
                              "--no-knapsack", "--seed", "--help"});
    if (arguments.has("--help") || !arguments.has("--input")) {
        print_usage();
        return arguments.has("--help") ? 0 : 1;
    }

    mockturtle::xag_network const network =
        lut_synth::command_line::read_verilog_network(arguments.text("--input"));

    lut_synth::approximate::ResubALSParams parameters;
    parameters.error_bound = arguments.real("--error-bound", 0.05);
    parameters.estimator = parse_estimator(arguments.text("--estimator", "integer"));
    parameters.num_patterns = arguments.number("--num-patterns", 102400);
    parameters.max_divisors = arguments.number("--max-divisors", 150);
    parameters.max_lac_size = arguments.number("--max-lac-size", 2);
    parameters.use_knapsack = !arguments.has("--no-knapsack");
    parameters.seed = arguments.number("--seed", 0);

    lut_synth::approximate::ResubALSResult const result =
        lut_synth::approximate::resubals(network, parameters);
    lut_synth::command_line::write_verilog_network(result.network,
                                                   arguments.text("--output"));

    lut_synth::command_line::Report()
        .add("method", std::string("resubals"))
        .add("estimator", arguments.text("--estimator", "integer"))
        .add("num_inputs", network.num_pis())
        .add("num_outputs", network.num_pos())
        .add("error_bound", parameters.error_bound)
        .add_delta(network, result.network)
        .add("lacs_applied", result.stats.lacs_applied)
        .add("rollbacks", result.stats.rollbacks)
        .add("actual_error", result.stats.actual_error)
        .add("verified", result.stats.verified)
        .print();
    return 0;
}
