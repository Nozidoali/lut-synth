#pragma once

#include <string>

namespace lut_synth { class XagSynthesizer; }

namespace lut_synth::command_line {

class Arguments;

/*! \brief Run one truth-table-to-Verilog synthesis and report the result.
 *
 *  Shared body of every `lut-synth-<method>` tool: reads the `--input`
 *  truth table, runs \p synthesizer over it, writes the `--output` Verilog
 *  when a path was given, and prints the JSON report. The tools differ only
 *  in which flags they accept and how they build their synthesizer.
 *
 *  \param arguments Parsed command line, expected to carry `--input`
 *  \param synthesizer Configured synthesis method
 *  \param method Method name as it should appear in the report
 *  \return Process exit status
 */
int run_synthesis(Arguments const &arguments, XagSynthesizer const &synthesizer,
                  std::string const &method);

} // namespace lut_synth::command_line
