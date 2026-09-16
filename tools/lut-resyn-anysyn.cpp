#include "command-line.hpp"
#include "lut-synth/resynthesis/resynthesis.hpp"

#include <iostream>

namespace {

void print_usage() {
    std::cerr <<
        "Usage: lut-resyn-anysyn --input <file.v> [--output <file.v>] [options]\n"
        "\n"
        "AnySyn cost-generic optimisation: alternating cost-generic\n"
        "resubstitution, minmc cut rewriting and LUT-based perturbation, run\n"
        "under a total time budget. Exact -- the function is preserved.\n"
        "\n"
        "  Wang, Lee and De Micheli, AnySyn: A Cost-Generic Logic Synthesis\n"
        "  Framework with Customizable Cost Functions, arXiv:2311.14721, 2023.\n"
        "\n"
        "Options:\n"
        "  --input <file.v>      Verilog netlist to optimise (required)\n"
        "  --output <file.v>     Write the optimised netlist\n"
        "  --timeout <seconds>   Total time budget (default: 60)\n"
        "  --rounds <N>          Optimisation rounds (default: 3)\n"
        "  --iterations <N>      Iterations per round (default: 5)\n"
        "  --cut-limit <N>       Cuts enumerated per node (default: 25)\n"
        "  --klut-sizes <a,b,c>  LUT sizes used for perturbation (default: 4,5,6)\n"
        "  --allow-zero-gain     Accept moves that leave the cost unchanged\n"
        "  --no-dont-cares       Disable don't-cares during cut rewriting\n"
        "  --dc-and-rewrite      Enable the ODC-aware AND rewrite pass\n";
}

} // namespace

int main(int argc, char *argv[]) {
    lut_synth::command_line::Arguments const arguments(argc, argv);
    arguments.reject_unknown({"--input", "--output", "--timeout", "--rounds",
                              "--iterations", "--cut-limit", "--klut-sizes",
                              "--allow-zero-gain", "--no-dont-cares",
                              "--dc-and-rewrite", "--help"});
    if (arguments.has("--help") || !arguments.has("--input")) {
        print_usage();
        return arguments.has("--help") ? 0 : 1;
    }

    mockturtle::xag_network const network =
        lut_synth::command_line::read_verilog_network(arguments.text("--input"));

    lut_synth::AnySynParams parameters;
    parameters.timeout_s = arguments.real("--timeout", 60.0);
    parameters.optimization_rounds = arguments.number("--rounds", 3);
    parameters.max_iterations_per_round = arguments.number("--iterations", 5);
    parameters.cut_limit = arguments.number("--cut-limit", 25);
    parameters.allow_zero_gain = arguments.has("--allow-zero-gain");
    parameters.use_dont_cares = !arguments.has("--no-dont-cares");
    parameters.use_dc_and_rewrite = arguments.has("--dc-and-rewrite");
    if (arguments.has("--klut-sizes")) {
        parameters.klut_sizes = arguments.number_list("--klut-sizes");
    }

    mockturtle::xag_network const optimized =
        lut_synth::resynthesize_xag_anysyn(network, parameters);
    lut_synth::command_line::write_verilog_network(optimized,
                                                   arguments.text("--output"));

    lut_synth::command_line::Report()
        .add("method", std::string("anysyn"))
        .add("num_inputs", network.num_pis())
        .add("num_outputs", network.num_pos())
        .add_delta(network, optimized)
        .print();
    return 0;
}
