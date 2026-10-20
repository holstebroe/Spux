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
spux_add_test(platform_window_test spux_platform)
spux_add_test(gl_context_test spux_ui_modern)
spux_add_test(alloc_guard_test spux_testing_allocguard)
spux_add_test(wav_test)
spux_add_test(loudness_test)
spux_add_test(metrics_test)
spux_add_test(hash_test)
spux_add_test(render_safety_test)
spux_add_test(null_test)
spux_add_test(midi_test spux_midi)

# spux_core and spux_dsp (header-only). The kernel test reads and, with SPUX_UPDATE_GOLDEN=1, writes
# tests/kernels/ in the source tree.
foreach(t math_test pcg32_test smoothing_test filters_test halfband_test blep_test saturator_test noise_drift_test
          dsp_kernel_test)
  spux_add_test(${t} spux_dsp)
  target_compile_definitions(${t} PRIVATE SPUX_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
endforeach()

# The CLAP smoke host against a small test plugin and four broken variants of it.
function(spux_test_plugin name fault)
  add_library(${name} MODULE clap_test_plugin.cpp)
  target_link_libraries(${name} PRIVATE clap)
  target_compile_definitions(${name} PRIVATE SPUX_TEST_FAULT=${fault})
  if(UNIX AND NOT APPLE AND SPUX_WITH_X11)
    target_compile_definitions(${name} PRIVATE SPUX_HAS_X11=1)
    target_include_directories(${name} PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(${name} PRIVATE ${X11_LIBRARIES})
  endif()
  set_target_properties(${name} PROPERTIES PREFIX "" SUFFIX ".clap")
  spux_target_flags(${name})
endfunction()
spux_test_plugin(spux_test_plugin_good 0)
spux_test_plugin(spux_test_plugin_nan 1)
spux_test_plugin(spux_test_plugin_loud 2)
spux_test_plugin(spux_test_plugin_state 3)
spux_test_plugin(spux_test_plugin_noparam 4)
spux_test_plugin(spux_test_plugin_badrange 5)
spux_add_test(clap_smoke_test spux_testing_clap)
foreach(variant good nan loud state noparam badrange)
  string(TOUPPER ${variant} up)
  target_compile_definitions(clap_smoke_test PRIVATE SPUX_TEST_PLUGIN_${up}="$<TARGET_FILE:spux_test_plugin_${variant}>")
  add_dependencies(clap_smoke_test spux_test_plugin_${variant})
endforeach()

# The command line smoke tool and the CMake helper that registers it, on the good plugin.
if(TARGET spux_clap_smoke)
  spux_add_clap_smoke_test(clap_smoke_tool spux_test_plugin_good ARGS --plugins 1 --params 2 --require-audio)
endif()

# Packaging helpers: deploy target, release zip and the installer hook, in a probe project.
add_test(NAME packaging_test
  COMMAND ${CMAKE_COMMAND} -DSPUX_CMAKE_DIR=${SPUX_ROOT}/cmake -DPROBE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/packaging
          -DWORK=${CMAKE_CURRENT_BINARY_DIR}/packaging_work -DGENERATOR=${CMAKE_GENERATOR}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/packaging_test.cmake)
