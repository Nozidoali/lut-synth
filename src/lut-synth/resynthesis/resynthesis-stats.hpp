#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lut_synth {

struct ResynthesisStepInfo {
    uint32_t step_number = 0;
    std::string step_type;
    uint32_t and_count_before = 0;
    uint32_t and_count_after = 0;
    uint32_t xor_count_before = 0;
    uint32_t xor_count_after = 0;
    bool improved = false;
    double time_ms = 0.0;
};

struct ResynthesisIterationInfo {
    uint32_t iteration = 0;
    uint32_t and_count_start = 0;
    uint32_t and_count_end = 0;
    uint32_t rewrite_steps = 0;
    bool esop_improved = false;
    double iteration_time_ms = 0.0;
    std::vector<ResynthesisStepInfo> steps;
};

struct ResynthesisReportData {
    uint32_t initial_and_count = 0;
    uint32_t final_and_count = 0;
    uint32_t initial_xor_count = 0;
    uint32_t final_xor_count = 0;
    uint32_t num_inputs = 0;
    uint32_t num_outputs = 0;
    uint32_t max_iterations = 0;
    uint32_t actual_iterations = 0;
    double total_time_ms = 0.0;
    std::vector<ResynthesisIterationInfo> iterations;
};

} // namespace lut_synth
