cmake_minimum_required(VERSION 3.16)
project(lut-synth LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(third-party/mockturtle EXCLUDE_FROM_ALL)

add_library(lut-synth STATIC
  src/lut-synth/truth-table.cpp
  src/lut-synth/error.cpp
  src/lut-synth/string-util.cpp
  src/lut-synth/synthesis/synthesis.cpp
  src/lut-synth/synthesis/xag-synthesizer.cpp
  src/lut-synth/synthesis/ss-synthesizer.cpp
  src/lut-synth/synthesis/exact-synthesis.cpp
  src/lut-synth/synthesis/anf-xag-builder.cpp
  src/lut-synth/synthesis/select-swap.cpp
  src/lut-synth/synthesis/select-swap-dont-care.cpp
  src/lut-synth/synthesis/davio.cpp
  src/lut-synth/synthesis/disjoint-support-decomposition.cpp
  src/lut-synth/synthesis/resubstitution.cpp
  src/lut-synth/synthesis/util.cpp
  src/lut-synth/synthesis/variable-selection.cpp
  src/lut-synth/resynthesis/xag-resynthesizer.cpp
  src/lut-synth/resynthesis/anysyn-resynthesizer.cpp
  src/lut-synth/resynthesis/cost-generic-resub.cpp
  src/lut-synth/resynthesis/resub-patterns.cpp
  src/lut-synth/resynthesis/resynthesis-util.cpp
  src/lut-synth/resynthesis/resynthesis-anysyn.cpp
  src/lut-synth/resynthesis/dc-and-rewrite.cpp
  src/lut-synth/approximate/resynthesis/resubals.cpp
  src/lut-synth/approximate/resynthesis/narrow-resub.cpp
  src/lut-synth/approximate/resynthesis/lac.cpp
  src/lut-synth/approximate/resynthesis/lac-manager.cpp
  src/lut-synth/approximate/resynthesis/knapsack.cpp
  src/lut-synth/approximate/resynthesis/error-estimator.cpp
  src/lut-synth/approximate/resynthesis/simulation-estimator.cpp
  src/lut-synth/approximate/resynthesis/vecbee-estimator.cpp
  src/lut-synth/approximate/resynthesis/integer-estimator.cpp
  src/lut-synth/approximate/resynthesis/miter-builder.cpp
  src/lut-synth/approximate/resynthesis/miter-estimator.cpp
  src/lut-synth/approximate/resynthesis/tt-ops.cpp
)

target_include_directories(lut-synth PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(lut-synth PUBLIC
  mockturtle
  kitty
)

option(BUILD_TOOLS "Build command-line tools" ON)
if(BUILD_TOOLS)
  add_library(lut-synth-command-line STATIC
    tools/command-line.cpp
    tools/synthesis-driver.cpp
  )
  target_include_directories(lut-synth-command-line PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
  )
  target_link_libraries(lut-synth-command-line PUBLIC lut-synth)

  # Truth table in, Verilog out: one binary per synthesis method.
  foreach(method ss davio dsd exact)
    add_executable(lut-synth-${method} tools/lut-synth-${method}.cpp)
    target_link_libraries(lut-synth-${method} PRIVATE lut-synth-command-line)
  endforeach()

  # Verilog in, Verilog out: one binary per resynthesis method.
  foreach(method anysyn narrow resubals)
    add_executable(lut-resyn-${method} tools/lut-resyn-${method}.cpp)
    target_link_libraries(lut-resyn-${method} PRIVATE lut-synth-command-line)
  endforeach()
endif()

option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
