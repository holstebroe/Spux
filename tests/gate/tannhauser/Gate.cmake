# Acceptance gate, Tannhauser (included by tests/gate/CMakeLists.txt). Tannhauser has one look, the Classic
# panel (its `modern/` folder is Classic); there is no Retro build to measure. Flags as in Tannhauser's
# CMakeLists.txt: -Os for the GUI and CLAP glue, link-time optimization, hidden visibility for the module.
set(T ${SPUX_GATE_TANNHAUSER_DIR})
add_library(tannhauser_gate_core OBJECT
  ${T}/src/core/Params.cpp ${T}/src/core/SynthEngine.cpp ${T}/src/core/Effects.cpp
  ${T}/src/presets/Presets.cpp ${T}/src/presets/PresetData.cpp)
target_include_directories(tannhauser_gate_core PRIVATE ${T}/clap/include ${T}/src)
spux_gate_plugin_flags(tannhauser_gate_core)
# Tannhauser is C++17 (u8string() is a std::string there); the port needs C++20: keep char8_t off.
target_compile_options(tannhauser_gate_core PRIVATE -fno-char8_t)
set_source_files_properties(${T}/src/presets/Presets.cpp ${T}/src/presets/PresetData.cpp
                            PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Release>:-Os>")

set(_tgui_common ${T}/src/gui/Font.cpp ${T}/src/gui/Graphics.cpp ${T}/src/gui/modern/ModernDraw.cpp
                 ${T}/src/gui/PanelLayout.cpp ${T}/src/gui/PanelRenderer.cpp)
foreach(variant retained immediate)
  if(variant STREQUAL "retained")
    set(gui ${_tgui_common} ${T}/src/gui/FileDialog.cpp ${T}/src/gui/GuiWindow.cpp)
    set(inc ${T}/clap/include ${T}/src ${T}/src/gui)
    set(libs "")
  else()
    set(gui ${_tgui_common} ${_gate}/tannhauser/GuiWindow.cpp)
    set(inc ${_gate}/tannhauser/include ${_gate} ${T}/clap/include ${T}/src ${T}/src/gui)
    set(libs spux_ui_core)
  endif()
  set_source_files_properties(${T}/src/gui/Font.cpp ${T}/src/gui/FileDialog.cpp ${T}/src/gui/PanelLayout.cpp
                              ${T}/src/gui/GuiWindow.cpp ${_gate}/tannhauser/GuiWindow.cpp ${T}/src/clap/TannhauserClap.cpp
                              PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Release>:-Os>")
  set(mod tannhauser_gate_${variant}_classic)
  add_library(${mod} MODULE $<TARGET_OBJECTS:tannhauser_gate_core> ${T}/src/clap/TannhauserClap.cpp ${gui})
  set(bench tannhauser_gate_bench_${variant}_classic)
  add_executable(${bench} $<TARGET_OBJECTS:tannhauser_gate_core> ${T}/src/clap/TannhauserClap.cpp ${gui}
                          ${_gate}/TannhauserBench.cpp)
  foreach(t ${mod} ${bench})
    target_include_directories(${t} PRIVATE ${inc})
    target_link_libraries(${t} PRIVATE ${libs} ${_gate_gui_libs})
    spux_gate_plugin_flags(${t})
    target_compile_options(${t} PRIVATE -fno-char8_t)
  endforeach()
  set_target_properties(${mod} PROPERTIES PREFIX "" SUFFIX ".clap" CXX_VISIBILITY_PRESET hidden C_VISIBILITY_PRESET hidden)
  target_compile_definitions(${bench} PRIVATE TANNHAUSER_GATE_VARIANT="${variant}")
  add_dependencies(spux_gate ${mod} ${bench})
endforeach()
