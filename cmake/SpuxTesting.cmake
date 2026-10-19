# Test helpers for plugins.
#
#   spux_add_clap_smoke_test(<name> <clap-target> [ARGS <spux_clap_smoke options>...])
#       ctest entry that loads the built plugin through the spux smoke host (spux_clap_smoke). Needs
#       -DSPUX_BUILD_SMOKE_TOOL=ON or the top-level spux build; the plugin repository calls it after enable_testing().
function(spux_add_clap_smoke_test name clapTarget)
  cmake_parse_arguments(A "" "" "ARGS" ${ARGN})
  if(NOT TARGET spux_clap_smoke)
    message(FATAL_ERROR "spux_add_clap_smoke_test: the spux_clap_smoke tool is not built (SPUX_BUILD_SMOKE_TOOL)")
  endif()
  add_test(NAME ${name} COMMAND $<TARGET_FILE:spux_clap_smoke> $<TARGET_FILE:${clapTarget}> ${A_ARGS})
  set_tests_properties(${name} PROPERTIES LABELS clap_smoke)
endfunction()
