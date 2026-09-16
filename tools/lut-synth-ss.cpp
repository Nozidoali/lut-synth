#include "command-line.hpp"
#include "synthesis-driver.hpp"
#include "lut-synth/synthesis/ss-synthesizer.hpp"

#include <iostream>

namespace {

void print_usage() {
    std::cerr <<
        "Usage: lut-synth-ss --input <file.tt> [--output <file.v>] [options]\n"
        "\n"
        "Select-swap synthesis: Shannon decomposition over k selector bits\n"
        "plus an ANF cover of each cofactor, chosen to minimise AND count.\n"
        "This is the only method that exploits don't-cares.\n"
        "\n"
        "Options:\n"
        "  --input <file.tt>        Truth table to synthesize (required)\n"
        "  --output <file.v>        Write the resulting XAG as Verilog\n"
        "  --k <int>                Shannon parameter, -1 selects the best k\n"
        "  --num-random-starts <N>  Random variable orderings to try\n"
        "  --seed <N>               Seed for the random orderings\n"
        "  --disable-dont-care      Treat don't-cares as zeros\n"
        "  --verbose                Print truth table statistics\n";
}

} // namespace

int main(int argc, char *argv[]) {
    lut_synth::command_line::Arguments const arguments(argc, argv);
    arguments.reject_unknown({"--input", "--output", "--k", "--num-random-starts",
                              "--seed", "--disable-dont-care", "--verbose",
                              "--help"});
    if (arguments.has("--help") || !arguments.has("--input")) {
        print_usage();
        return arguments.has("--help") ? 0 : 1;
    }

    lut_synth::SSSynthesizer const synthesizer(
        arguments.signed_number("--k", -1),
        arguments.number("--num-random-starts", 1),
        arguments.large_number("--seed", 0),
        arguments.has("--disable-dont-care"));

    return lut_synth::command_line::run_synthesis(arguments, synthesizer, "ss");
}
