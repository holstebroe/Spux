cmake_minimum_required(VERSION 3.21)
project(spux VERSION 0.1.0 LANGUAGES C CXX)

# A plugin adds spux as extern/spux, or from a local checkout while developing:
#   if(SPUX_SOURCE_DIR)
#     add_subdirectory(${SPUX_SOURCE_DIR} spux)   # -DSPUX_SOURCE_DIR=/path/to/spux
#   else()
#     add_subdirectory(extern/spux)
#   endif()
# spux itself only uses its own directory (CMAKE_CURRENT_LIST_DIR), so either works.
set(SPUX_ROOT ${CMAKE_CURRENT_LIST_DIR})

if(PROJECT_IS_TOP_LEVEL)
  set(_spux_dev ON)
else()
  set(_spux_dev OFF)
endif()
option(SPUX_BUILD_TESTS "Build the spux tests" ${_spux_dev})
option(SPUX_BUILD_CANARY "Build the size canary plugin" ${_spux_dev})
option(SPUX_WITH_X11 "Linux: build the X11 window and GLX context. OFF for headless builds without X11 headers; windows then report no support" ON)
option(SPUX_BUILD_SMOKE_TOOL "Build spux_clap_smoke, the command line CLAP smoke host" ${_spux_dev})
option(SPUX_BUILD_BENCH "Build spux_bench (math against libm, DSP block timings; meaningful in Release)" ${_spux_dev})
option(SPUX_BUILD_VST3 "Make clap-wrapper's make_clapfirst_plugins available (downloads the VST3 SDK)" OFF)
# How spux's UI code is optimized (DESIGN_GUIDELINES 4, item 9). SIZE: -Os (MSVC /O1); SPEED: the build type's
# flags. AUTO picks SIZE when the plugin builds the Retro flavor and SPEED otherwise; the plugin tells spux its
# flavor with SPUX_UI_FLAVOR (set it before add_subdirectory, or on the command line).
set(SPUX_UI_FLAVOR "" CACHE STRING "The plugin's UI flavor: RETRO, CLASSIC, MODERN or NONE (for SPUX_UI_OPTIMIZE=AUTO)")
set(SPUX_UI_OPTIMIZE "AUTO" CACHE STRING "Optimize spux's UI code for SIZE or SPEED; AUTO: SIZE for Retro, SPEED otherwise")
set_property(CACHE SPUX_UI_OPTIMIZE PROPERTY STRINGS AUTO SIZE SPEED)

# The plugin chooses its language level; spux needs C++20 at least. The top-level build sets it.
if(_spux_dev)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
  endif()
endif()

include(${SPUX_ROOT}/cmake/SpuxFlags.cmake)
include(${SPUX_ROOT}/cmake/SpuxPackaging.cmake)
include(${SPUX_ROOT}/cmake/SpuxTesting.cmake)

# Third party (spux owns the versions): clap, clap-wrapper (author's fork), doctest.
if(NOT TARGET clap)
  add_subdirectory(${SPUX_ROOT}/third_party/clap ${CMAKE_CURRENT_BINARY_DIR}/third_party/clap EXCLUDE_FROM_ALL)
endif()
if(SPUX_BUILD_VST3 AND NOT COMMAND make_clapfirst_plugins)
  set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "")
  set(CLAP_WRAPPER_BUILD_AUV2 OFF CACHE BOOL "")
  add_subdirectory(${SPUX_ROOT}/third_party/clap-wrapper ${CMAKE_CURRENT_BINARY_DIR}/third_party/clap-wrapper)
endif()
add_library(spux_doctest INTERFACE)
target_include_directories(spux_doctest SYSTEM INTERFACE ${SPUX_ROOT}/third_party/doctest/doctest)

# Modules (README table). Nothing is compiled unless a plugin links the module. A module is an
# INTERFACE library until it has sources; it becomes STATIC then, which changes nothing for plugins.
function(spux_module name)
  cmake_parse_arguments(M "" "" "DEPENDS" ${ARGN})
  add_library(${name} INTERFACE)
  add_library(spux::${name} ALIAS ${name})
  target_include_directories(${name} INTERFACE ${SPUX_ROOT}/include)
  target_compile_features(${name} INTERFACE cxx_std_20)
  if(M_DEPENDS)
    target_link_libraries(${name} INTERFACE ${M_DEPENDS})
  endif()
endfunction()

spux_module(spux_core)                                       # header-only
spux_module(spux_dsp DEPENDS spux_core)                      # header-only
spux_module(spux_params DEPENDS spux_core)
spux_module(spux_state DEPENDS spux_core)
spux_module(spux_clap DEPENDS spux_params spux_state clap)
spux_module(spux_midi DEPENDS spux_core)
spux_module(spux_ui_retro DEPENDS spux_ui_core)
spux_module(spux_presetbrowser DEPENDS spux_ui_core spux_state)
spux_module(spux_testing DEPENDS spux_core spux_dsp spux_doctest)    # header-only: WAV, loudness, metrics, hashes, null test

# spux_testing_allocguard: replaces the global operator new/delete to catch allocations on the audio thread.
# Test executables only, never a plugin (the replacement is process-wide).
add_library(spux_testing_allocguard STATIC ${SPUX_ROOT}/src/testing/AllocGuard.cpp)
add_library(spux::spux_testing_allocguard ALIAS spux_testing_allocguard)
target_include_directories(spux_testing_allocguard PUBLIC ${SPUX_ROOT}/include)
target_compile_features(spux_testing_allocguard PUBLIC cxx_std_20)
set_target_properties(spux_testing_allocguard PROPERTIES POSITION_INDEPENDENT_CODE ON EXCLUDE_FROM_ALL ON)
spux_target_flags(spux_testing_allocguard)

# spux_testing_clap: the CLAP smoke host (load a .clap, drive it like a DAW). Test executables only.
add_library(spux_testing_clap STATIC ${SPUX_ROOT}/src/testing/ClapHost.cpp)
add_library(spux::spux_testing_clap ALIAS spux_testing_clap)
target_include_directories(spux_testing_clap PUBLIC ${SPUX_ROOT}/include)
target_compile_features(spux_testing_clap PUBLIC cxx_std_20)
target_link_libraries(spux_testing_clap PUBLIC clap ${CMAKE_DL_LIBS})
set_target_properties(spux_testing_clap PROPERTIES POSITION_INDEPENDENT_CODE ON EXCLUDE_FROM_ALL ON)
if(UNIX AND NOT APPLE AND SPUX_WITH_X11)
  find_package(X11 REQUIRED)
  target_compile_definitions(spux_testing_clap PRIVATE SPUX_HAS_X11=1)
  target_include_directories(spux_testing_clap PRIVATE ${X11_INCLUDE_DIR})
  target_link_libraries(spux_testing_clap PUBLIC ${X11_LIBRARIES})
endif()
spux_target_flags(spux_testing_clap)

if(SPUX_BUILD_SMOKE_TOOL)
  add_executable(spux_clap_smoke ${SPUX_ROOT}/tools/ClapSmoke.cpp)
  target_link_libraries(spux_clap_smoke PRIVATE spux_testing_clap)
  spux_target_flags(spux_clap_smoke)
endif()

# Applies SPUX_UI_OPTIMIZE to a UI target (not in Debug builds).
function(spux_ui_optimize target)
  string(TOUPPER "${SPUX_UI_OPTIMIZE}" mode)
  string(TOUPPER "${SPUX_UI_FLAVOR}" flavor)
  if(mode STREQUAL "AUTO")
    if(flavor STREQUAL "RETRO")
      set(mode SIZE)
    else()
      set(mode SPEED)
    endif()
  endif()
  if(mode STREQUAL "SIZE")
    if(MSVC)
      target_compile_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:/O1>)
    else()
      target_compile_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-Os>)
    endif()
  elseif(NOT mode STREQUAL "SPEED")
    message(FATAL_ERROR "SPUX_UI_OPTIMIZE must be AUTO, SIZE or SPEED, not '${SPUX_UI_OPTIMIZE}'")
  endif()
endfunction()

# spux_ui_core: immediate-mode Gui, IRenderer, the CPU layer cache, painter interfaces, Theme, EditorHost,
# the editor shell and the telemetry consumers. STATIC: a plugin pays only for the objects it references.
add_library(spux_ui_core STATIC
  ${SPUX_ROOT}/src/ui/core/CpuLayers.cpp
  ${SPUX_ROOT}/src/ui/core/Gui.cpp
  ${SPUX_ROOT}/src/ui/core/Editor.cpp
  ${SPUX_ROOT}/src/ui/core/Live.cpp)
add_library(spux::spux_ui_core ALIAS spux_ui_core)
target_include_directories(spux_ui_core PUBLIC ${SPUX_ROOT}/include)
target_compile_features(spux_ui_core PUBLIC cxx_std_20)
target_link_libraries(spux_ui_core PUBLIC spux_platform spux_params spux_dsp)
set_target_properties(spux_ui_core PROPERTIES POSITION_INDEPENDENT_CODE ON EXCLUDE_FROM_ALL ON)
spux_target_flags(spux_ui_core)
spux_ui_optimize(spux_ui_core)

# spux_ui_classic: Classic drawing code (Graphics, Font, FileDialog, ClassicDraw), the first module with
# sources, so it is a STATIC library. CPU drawing only. The font data headers (DisplayFont.hpp,
# LabelFont.hpp) are headers a plugin includes where it needs them; nothing else is compiled in.
add_library(spux_ui_classic STATIC
  ${SPUX_ROOT}/src/ui/classic/Font.cpp
  ${SPUX_ROOT}/src/ui/classic/Graphics.cpp
  ${SPUX_ROOT}/src/ui/classic/FileDialog.cpp
  ${SPUX_ROOT}/src/ui/classic/ClassicDraw.cpp)
add_library(spux::spux_ui_classic ALIAS spux_ui_classic)
target_include_directories(spux_ui_classic PUBLIC ${SPUX_ROOT}/include)
target_compile_features(spux_ui_classic PUBLIC cxx_std_20)
target_link_libraries(spux_ui_classic PUBLIC spux_ui_core)
set_target_properties(spux_ui_classic PROPERTIES POSITION_INDEPENDENT_CODE ON)
spux_target_flags(spux_ui_classic)
spux_ui_optimize(spux_ui_classic)

# spux_platform: native child window (X11, Win32, none elsewhere). Sources, so STATIC; it is not part of
# `all` of a plugin that never links it. The OS libraries are public because a plugin links the window
# code. The GLX visual lookup opens libGL at run time, so no GL library is needed here.
add_library(spux_platform STATIC
  ${SPUX_ROOT}/src/platform/NativeWindow_x11.cpp
  ${SPUX_ROOT}/src/platform/NativeWindow_win32.cpp
  ${SPUX_ROOT}/src/platform/NativeWindow_other.cpp
  ${SPUX_ROOT}/src/platform/GlxVisual_x11.cpp)
add_library(spux::spux_platform ALIAS spux_platform)
target_include_directories(spux_platform PUBLIC ${SPUX_ROOT}/include)
target_include_directories(spux_platform PRIVATE ${SPUX_ROOT}/src/platform)
target_compile_features(spux_platform PUBLIC cxx_std_20)
set_target_properties(spux_platform PROPERTIES POSITION_INDEPENDENT_CODE ON EXCLUDE_FROM_ALL ON)
if(UNIX AND NOT APPLE AND SPUX_WITH_X11)
  find_package(X11 REQUIRED)
  find_package(Threads REQUIRED)
  target_compile_definitions(spux_platform PUBLIC SPUX_HAS_X11=1)
  target_include_directories(spux_platform PUBLIC ${X11_INCLUDE_DIR})
  target_link_libraries(spux_platform PUBLIC ${X11_LIBRARIES} Threads::Threads ${CMAKE_DL_LIBS})
elseif(WIN32)
  target_link_libraries(spux_platform PUBLIC user32 gdi32)
endif()
spux_target_flags(spux_platform)
spux_ui_optimize(spux_platform)

# spux_ui_modern: the Modern flavor. The GL function table and context (loaded at run time), the GPU renderer
# with its shaders and the distance-field font atlas, the software renderer (the same style on machines
# without a usable GPU), ModernStyle (painters configured by a ModernConfig) and the generic views. STATIC,
# one object per part, so a plugin links only what it references; the font data is its own object.
# OpenGL is opened at run time on Linux and Windows.
include(${SPUX_ROOT}/cmake/SpuxEmbedShaders.cmake)
spux_embed_shaders(_spux_shader_source ${SPUX_ROOT}/src/ui/modern/shaders
                   ${CMAKE_CURRENT_BINARY_DIR}/spux_generated/ShaderSources.cpp)
add_library(spux_ui_modern STATIC
  ${SPUX_ROOT}/src/gl/GlApi.cpp
  ${SPUX_ROOT}/src/gl/GlContext_glx.cpp
  ${SPUX_ROOT}/src/gl/GlContext_wgl.cpp
  ${SPUX_ROOT}/src/gl/GlContext_none.cpp
  ${SPUX_ROOT}/src/ui/modern/FontAtlas.cpp
  ${SPUX_ROOT}/src/ui/modern/InterFont.cpp
  ${SPUX_ROOT}/src/ui/modern/GpuRenderer.cpp
  ${SPUX_ROOT}/src/ui/modern/SoftRenderer.cpp
  ${SPUX_ROOT}/src/ui/modern/ModernSurface.cpp
  ${SPUX_ROOT}/src/ui/modern/ModernStyle.cpp
  ${SPUX_ROOT}/src/ui/modern/ModernViews.cpp
  ${_spux_shader_source})
add_library(spux::spux_ui_modern ALIAS spux_ui_modern)
target_include_directories(spux_ui_modern PUBLIC ${SPUX_ROOT}/include)
target_include_directories(spux_ui_modern PRIVATE ${SPUX_ROOT}/src/ui/modern)
target_include_directories(spux_ui_modern SYSTEM PRIVATE ${SPUX_ROOT}/third_party)
target_compile_features(spux_ui_modern PUBLIC cxx_std_20)
target_link_libraries(spux_ui_modern PUBLIC spux_ui_core)
if(WIN32)
  target_link_libraries(spux_ui_modern PUBLIC opengl32)
endif()
set_target_properties(spux_ui_modern PROPERTIES POSITION_INDEPENDENT_CODE ON EXCLUDE_FROM_ALL ON)
spux_target_flags(spux_ui_modern)
spux_ui_optimize(spux_ui_modern)

if(SPUX_BUILD_BENCH)
  add_executable(spux_bench ${SPUX_ROOT}/bench/SpuxBench.cpp)
  target_link_libraries(spux_bench PRIVATE spux_dsp)
  spux_target_flags(spux_bench)
endif()

if(SPUX_BUILD_TESTS)
  enable_testing()
  add_subdirectory(${SPUX_ROOT}/tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()
if(SPUX_BUILD_CANARY)
  add_subdirectory(${SPUX_ROOT}/canary ${CMAKE_CURRENT_BINARY_DIR}/canary)
endif()
