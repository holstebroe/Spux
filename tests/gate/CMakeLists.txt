# TEMPORARY, to be removed: the immediate-mode acceptance gate (docs/UI_GATE.md). It ports two concrete
# plugin panels to measure spux_ui_core against their retained editors; the decision it served is made, and
# the whole folder (with tools/gate_report.sh and tools/gate_lines.py) goes once the plugins move onto
# spux_ui_core (docs/MIGRATION.md step 7). Nothing in the library depends on it. Built only when the plugin
# checkouts are given; the plugins' code stays in their repositories and is compiled from there:
#
#   cmake -S . -B build-gate -DCMAKE_BUILD_TYPE=Release \
#         -DSPUX_GATE_ACIDUS_DIR=/path/to/Acidus -DSPUX_GATE_TANNHAUSER_DIR=/path/to/Tannhauser
#   cmake --build build-gate --target spux_gate
#
# For each plugin every variant links the whole plugin (DSP, CLAP glue, GUI), compiled with the plugin's
# own settings (C++20 here, -Os for GUI and CLAP code, link-time optimization in Release), so the sizes
# compare like for like. Variants: `retained` (the plugin's own GuiWindow) and `immediate` (the port in
# tests/gate/<plugin>/ on spux_ui_core). Targets:
#   <plugin>_gate_<variant>_<style>        the plugin module (.clap), for the binary size
#   <plugin>_gate_bench_<variant>_<style>  CPU per frame and the pixel check (tests/gate/*Bench.cpp)
# tools/gate_report.sh runs them all and prints the table.

add_custom_target(spux_gate)

# Size experiment: link with section garbage collection, as spux_target_flags does for a plugin. spux's own UI
# code follows SPUX_UI_OPTIMIZE (configure with -DSPUX_UI_OPTIMIZE=SIZE or SPEED to compare).
option(SPUX_GATE_GC_SECTIONS "Gate modules: link with --gc-sections (both variants)" OFF)
set(_gate ${CMAKE_CURRENT_SOURCE_DIR})

include(CheckIPOSupported)
check_ipo_supported(RESULT _gate_ipo OUTPUT _gate_ipo_error)

function(spux_gate_plugin_flags target)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
                                             POSITION_INDEPENDENT_CODE ON)
  if(_gate_ipo AND CMAKE_BUILD_TYPE MATCHES "Release")
    set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
  if(SPUX_GATE_GC_SECTIONS)
    target_link_options(${target} PRIVATE -Wl,--gc-sections)
  endif()
endfunction()

if(UNIX AND NOT APPLE)
  find_package(X11 REQUIRED)
  set(_gate_gui_libs ${X11_LIBRARIES})
endif()

# --- Acidus -------------------------------------------------------------------------------------------------
if(SPUX_GATE_ACIDUS_DIR)
  set(A ${SPUX_GATE_ACIDUS_DIR})
  add_library(acidus_gate_core OBJECT
    ${A}/src/core/SynthEngine.cpp ${A}/src/core/Oscillator.cpp ${A}/src/core/Filter.cpp ${A}/src/core/Envelope.cpp
    ${A}/src/core/Distortion.cpp ${A}/src/core/CalibrationProfile.cpp)
  target_include_directories(acidus_gate_core PRIVATE ${A}/clap/include ${A}/src)
  target_link_libraries(acidus_gate_core PRIVATE spux_dsp)
  spux_gate_plugin_flags(acidus_gate_core)

  foreach(style RETRO MODERN)
    if(style STREQUAL "RETRO")
      set(skin ${A}/src/gui/RetroSkin.cpp)
      set(sname retro)
    else()
      set(skin ${A}/src/gui/modern/ModernSkin.cpp ${A}/src/gui/modern/ModernDraw.cpp)
      set(sname classic) # Acidus's MODERN skin is the Classic flavor (U1)
    endif()
    foreach(variant retained immediate)
      if(variant STREQUAL "retained")
        set(gui ${A}/src/gui/Font.cpp ${A}/src/gui/Graphics.cpp ${A}/src/gui/FileDialog.cpp ${A}/src/gui/GuiWindow.cpp ${skin})
        set(inc ${A}/clap/include ${A}/src ${A}/src/gui)
        set(libs "")
      else()
        # RetroSkin.cpp includes "GuiWindow.hpp" from its own folder: compile a copy next to a header that
        # forwards to the port's.
        set(copydir ${CMAKE_CURRENT_BINARY_DIR}/acidus_${sname})
        file(MAKE_DIRECTORY ${copydir})
        file(WRITE ${copydir}/GuiWindow.hpp "#include \"gui/GuiWindow.hpp\"\n")
        set(skincopy "")
        foreach(f ${skin})
          get_filename_component(n ${f} NAME)
          configure_file(${f} ${copydir}/${n} COPYONLY)
          list(APPEND skincopy ${copydir}/${n})
        endforeach()
        set(gui ${A}/src/gui/Font.cpp ${A}/src/gui/Graphics.cpp ${A}/src/gui/FileDialog.cpp ${skincopy}
                ${_gate}/acidus/GuiWindow.cpp ${_gate}/acidus/LogoPlate.cpp)
        set(inc ${_gate}/acidus/include ${_gate}/acidus ${_gate} ${A}/clap/include ${A}/src ${A}/src/gui ${A}/src/gui/modern)
        set(libs spux_ui_core)
      endif()
      set_source_files_properties(${gui} ${A}/src/clap/AcidusClap.cpp
                                  PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Release>:-Os>")
      set(mod acidus_gate_${variant}_${sname})
      add_library(${mod} MODULE $<TARGET_OBJECTS:acidus_gate_core> ${A}/src/clap/AcidusClap.cpp ${gui})
      set(bench acidus_gate_bench_${variant}_${sname})
      add_executable(${bench} $<TARGET_OBJECTS:acidus_gate_core> ${A}/src/clap/AcidusClap.cpp ${gui} ${_gate}/AcidusBench.cpp)
      foreach(t ${mod} ${bench})
        target_include_directories(${t} PRIVATE ${inc})
        target_link_libraries(${t} PRIVATE spux_dsp ${libs} ${_gate_gui_libs})
        if(style STREQUAL "MODERN")
          target_compile_definitions(${t} PRIVATE ACIDUS_GUI_STYLE_MODERN=1)
        endif()
        spux_gate_plugin_flags(${t})
      endforeach()
      set_target_properties(${mod} PROPERTIES PREFIX "" SUFFIX ".clap")
      target_compile_definitions(${bench} PRIVATE ACIDUS_GATE_VARIANT="${variant}" ACIDUS_GATE_STYLE="${sname}")
      add_dependencies(spux_gate ${mod} ${bench})
    endforeach()
  endforeach()
endif()

# --- Tannhauser ---------------------------------------------------------------------------------------------
if(SPUX_GATE_TANNHAUSER_DIR)
  include(${_gate}/tannhauser/Gate.cmake)
endif()
