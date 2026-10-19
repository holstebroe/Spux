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
option(SPUX_BUILD_VST3 "Make clap-wrapper's make_clapfirst_plugins available (downloads the VST3 SDK)" OFF)

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

# Third party (spux owns the versions, L6): clap, clap-wrapper (author's fork, L7), doctest.
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
spux_module(spux_platform)
spux_module(spux_ui_core DEPENDS spux_platform spux_params)
spux_module(spux_ui_retro DEPENDS spux_ui_core)
spux_module(spux_ui_classic DEPENDS spux_ui_core)
spux_module(spux_ui_modern DEPENDS spux_ui_core)
spux_module(spux_presetbrowser DEPENDS spux_ui_core spux_state)
spux_module(spux_testing DEPENDS spux_core spux_doctest)

if(SPUX_BUILD_TESTS)
  enable_testing()
  add_subdirectory(${SPUX_ROOT}/tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()
if(SPUX_BUILD_CANARY)
  add_subdirectory(${SPUX_ROOT}/canary ${CMAKE_CURRENT_BINARY_DIR}/canary)
endif()
