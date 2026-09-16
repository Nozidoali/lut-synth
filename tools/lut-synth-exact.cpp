#include "command-line.hpp"
#include "synthesis-driver.hpp"
#include "lut-synth/synthesis/exact-synthesizer.hpp"

#include <iostream>

namespace {

void print_usage() {
    std::cerr <<
        "Usage: lut-synth-exact --input <file.tt> [--output <file.v>] [options]\n"
        "\n"
        "SAT-based exact synthesis. Finds the minimum AND count, but the search\n"
        "is exponential and only practical for roughly six inputs or fewer.\n"
        "\n"
        "Options:\n"
        "  --input <file.tt>   Truth table to synthesize (required)\n"
        "  --output <file.v>   Write the resulting XAG as Verilog\n"
        "  --max-vars <N>      Refuse inputs wider than this (default: 6)\n"
        "  --no-cegar          Disable counterexample-guided refinement\n"
        "  --verbose           Print truth table and solver statistics\n";
}

} // namespace

int main(int argc, char *argv[]) {
    lut_synth::command_line::Arguments const arguments(argc, argv);
    arguments.reject_unknown({"--input", "--output", "--max-vars", "--no-cegar",
                              "--verbose", "--help"});
    if (arguments.has("--help") || !arguments.has("--input")) {
        print_usage();
        return arguments.has("--help") ? 0 : 1;
    }

    lut_synth::ExactSynthesisParams parameters;
    parameters.max_vars = arguments.number("--max-vars", 6);
    parameters.use_cegar = !arguments.has("--no-cegar");
    parameters.verbose = arguments.has("--verbose");

    lut_synth::ExactSynthesizer const synthesizer(parameters);
    return lut_synth::command_line::run_synthesis(arguments, synthesizer, "exact");
}
