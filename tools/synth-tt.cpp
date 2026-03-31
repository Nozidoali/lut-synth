#include "lut-synth/resynthesis/resynthesis-util.hpp"
#include "lut-synth/synthesis/ss-synthesizer.hpp"
#include "lut-synth/truth-table.hpp"

#include <cstdint>
#include <iostream>
#include <string>

namespace {

struct Args {
    std::string input;
    uint32_t num_random_starts = 1;
    uint64_t seed = 0;
    bool disable_dont_care = false;
    bool verbose = false;
};

Args parse_args(int argc, char* argv[]) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if ((arg == "--input" || arg == "-i") && i + 1 < argc) {
            args.input = argv[++i];
        } else if (arg == "--num-random-starts" && i + 1 < argc) {
            args.num_random_starts = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--seed" && i + 1 < argc) {
            args.seed = std::stoull(argv[++i]);
        } else if (arg == "--disable-dont-care") {
            args.disable_dont_care = true;
        } else if (arg == "--verbose" || arg == "-v") {
            args.verbose = true;
        }
    }
    return args;
}

void print_usage() {
    std::cerr << "Usage: synth-tt --input <file.tt> "
              << "[--num-random-starts N] [--seed S] "
              << "[--disable-dont-care] [--verbose]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    Args args = parse_args(argc, argv);
    if (args.input.empty()) {
        print_usage();
        return 1;
    }

    lut_synth::TruthTable tt;
    tt.read(args.input);

    uint32_t num_inputs = tt.get_tts()[0].num_vars();
    uint32_t num_outputs = static_cast<uint32_t>(tt.size());
    bool has_dc = tt.has_dont_cares();

    lut_synth::SSSynthesizer synth(-1, args.num_random_starts, args.seed,
                                    args.disable_dont_care);
    mockturtle::xag_network xag = synth.synthesize(tt);
    uint32_t and_count = lut_synth::count_ands(xag);

    std::string method = (has_dc && !args.disable_dont_care) ? "ss-dc" : "ss";

    if (args.verbose) {
        tt.print_stats();
    }

    std::cout << "{"
              << "\"num_inputs\":" << num_inputs
              << ",\"num_outputs\":" << num_outputs
              << ",\"has_dont_cares\":" << (has_dc ? "true" : "false")
              << ",\"and_count\":" << and_count
              << ",\"method\":\"" << method << "\""
              << "}\n";

    return 0;
}
