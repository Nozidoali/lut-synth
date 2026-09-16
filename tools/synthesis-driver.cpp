#include "synthesis-driver.hpp"

#include "command-line.hpp"
#include "lut-synth/resynthesis/resynthesis-util.hpp"
#include "lut-synth/synthesis/xag-synthesizer.hpp"
#include "lut-synth/truth-table.hpp"

namespace lut_synth::command_line {

int run_synthesis(Arguments const &arguments, XagSynthesizer const &synthesizer,
                  std::string const &method) {
    TruthTable const table = read_truth_table(arguments.text("--input"));
    if (arguments.has("--verbose")) table.print_stats();

    mockturtle::xag_network const network = synthesizer.synthesize(table);
    write_verilog_network(network, arguments.text("--output"));

    bool const exploited_dont_cares =
        synthesizer.supports_dont_care() && table.has_dont_cares();

    Report()
        .add("method", exploited_dont_cares ? method + "-dc" : method)
        .add("num_inputs", table.get_tts()[0].num_vars())
        .add("num_outputs", static_cast<uint32_t>(table.size()))
        .add("has_dont_cares", table.has_dont_cares())
        .add("size", network.num_gates())
        .add("and_count", count_ands(network))
        .print();
    return 0;
}

} // namespace lut_synth::command_line
