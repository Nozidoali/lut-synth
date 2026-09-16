#include "command-line.hpp"
#include "synthesis-driver.hpp"
#include "lut-synth/synthesis/davio-synthesizer.hpp"

#include <iostream>

namespace {

void print_usage() {
    std::cerr <<
        "Usage: lut-synth-davio --input <file.tt> [--output <file.v>] [options]\n"
        "\n"
        "Positive Davio decomposition. Fast, and a good fit for functions with\n"
        "XOR structure, but generally produces more ANDs than select-swap.\n"
        "\n"
        "Options:\n"
        "  --input <file.tt>   Truth table to synthesize (required)\n"
        "  --output <file.v>   Write the resulting XAG as Verilog\n"
        "  --verbose           Print truth table statistics\n";
}

} // namespace

int main(int argc, char *argv[]) {
    lut_synth::command_line::Arguments const arguments(argc, argv);
    arguments.reject_unknown({"--input", "--output", "--verbose", "--help"});
    if (arguments.has("--help") || !arguments.has("--input")) {
        print_usage();
        return arguments.has("--help") ? 0 : 1;
    }

    lut_synth::DavioSynthesizer const synthesizer;
    return lut_synth::command_line::run_synthesis(arguments, synthesizer, "davio");
}
