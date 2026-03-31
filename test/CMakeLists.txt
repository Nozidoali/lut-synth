set(CATCH2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/mockturtle/test/catch2")

function(add_lut_synth_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE lut-synth)
  target_include_directories(${name} PRIVATE ${CATCH2_INCLUDE_DIR})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_lut_synth_test(test-synthesis test-synthesis.cpp)
add_lut_synth_test(test-resynthesis test-resynthesis.cpp)
add_lut_synth_test(test-exact-synthesis test-exact-synthesis.cpp)
add_lut_synth_test(test-rmdds test-rmdds.cpp)
add_lut_synth_test(test-approximate test-approximate.cpp)
