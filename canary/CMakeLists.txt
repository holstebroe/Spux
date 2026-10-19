# Size canary (DESIGN_GUIDELINES section 4, item 7): an empty Retro-sized CLAP plugin. It links the
# modules a Retro plugin links, so as they gain code the stripped size of this binary shows what a
# Retro build pays for. It has no plugin inside, only the entry point.
add_library(spux_size_canary MODULE canary.cpp)
target_link_libraries(spux_size_canary PRIVATE spux_clap spux_ui_retro spux_dsp)
spux_target_flags(spux_size_canary)
set_target_properties(spux_size_canary PROPERTIES PREFIX "" OUTPUT_NAME "SpuxSizeCanary" SUFFIX ".clap")
if(NOT MSVC AND NOT APPLE)
  target_link_options(spux_size_canary PRIVATE -Wl,--strip-all) # the number we track is stripped
endif()

# `cmake --build . --target spux_size_canary_report` prints the size and checks canary/budget.cmake.
add_custom_target(spux_size_canary_report
  COMMAND ${CMAKE_COMMAND} -DCANARY=$<TARGET_FILE:spux_size_canary> -DBUDGET=${CMAKE_CURRENT_SOURCE_DIR}/budget.cmake
          -P ${SPUX_ROOT}/cmake/SizeCanary.cmake
  DEPENDS spux_size_canary VERBATIM)
