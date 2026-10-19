# Each test is its own executable and a ctest entry (doctest, L14).
add_library(spux_test_main STATIC test_main.cpp)
target_link_libraries(spux_test_main PUBLIC spux_testing)
spux_target_flags(spux_test_main)

function(spux_add_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE spux_test_main ${ARGN})
  spux_target_flags(${name})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

spux_add_test(version_test spux_core)
spux_add_test(determinism_flags_test spux_core)
spux_add_test(ui_classic_golden_test spux_ui_classic)
