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
  src/lut-synth/synthesis/rmdds.cpp
  src/lut-synth/synthesis/select-swap.cpp
  src/lut-synth/synthesis/select-swap-dont-care.cpp
  src/lut-synth/synthesis/affine-classification.cpp
  src/lut-synth/synthesis/davio.cpp
  src/lut-synth/synthesis/disjoint-support-decomposition.cpp
  src/lut-synth/synthesis/resubstitution.cpp
  src/lut-synth/synthesis/util.cpp
  src/lut-synth/synthesis/variable-selection.cpp
  src/lut-synth/resynthesis/xag-resynthesizer.cpp
  src/lut-synth/resynthesis/v3-resynthesizer.cpp
  src/lut-synth/resynthesis/cost-generic-resub.cpp
  src/lut-synth/resynthesis/resynthesis-util.cpp
  src/lut-synth/resynthesis/resynthesis-v3.cpp
  src/lut-synth/approximate/resynthesis/resubals.cpp
  src/lut-synth/approximate/resynthesis/lac.cpp
  src/lut-synth/approximate/resynthesis/lac-manager.cpp
  src/lut-synth/approximate/resynthesis/knapsack.cpp
  src/lut-synth/approximate/resynthesis/error-estimator.cpp
  src/lut-synth/approximate/resynthesis/simulation-estimator.cpp
  src/lut-synth/approximate/resynthesis/vecbee-estimator.cpp
  src/lut-synth/approximate/resynthesis/integer-estimator.cpp
  src/lut-synth/approximate/resynthesis/miter-estimator.cpp
  src/lut-synth/approximate/resynthesis/tt-ops.cpp
  src/lut-synth/approximate/tt-approximation/anf-data.cpp
  src/lut-synth/approximate/tt-approximation/ilp-common.cpp
  src/lut-synth/approximate/tt-approximation/tt-approximation.cpp
  src/lut-synth/approximate/tt-approximation/bounds.cpp
  src/lut-synth/approximate/tt-approximation/exact.cpp
)

target_include_directories(lut-synth PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(lut-synth PUBLIC
  mockturtle
  kitty
)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(GUROBI QUIET)
if(GUROBI_FOUND)
  target_compile_definitions(lut-synth PUBLIC HAS_GUROBI ENABLE_GUROBI)
  target_link_libraries(lut-synth PUBLIC ${GUROBI_LIBRARIES})
  target_include_directories(lut-synth PUBLIC ${GUROBI_INCLUDE_DIRS})
endif()

option(BUILD_TOOLS "Build command-line tools" OFF)
if(BUILD_TOOLS)
  add_executable(approx-tt tools/approx-tt.cpp)
  target_link_libraries(approx-tt PRIVATE lut-synth)

  add_executable(synth-tt tools/synth-tt.cpp)
  target_link_libraries(synth-tt PRIVATE lut-synth)
endif()

enable_testing()
add_subdirectory(test)
