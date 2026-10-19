# A tiny plugin project that uses the packaging helpers; driven by tests/packaging_test.cmake.
cmake_minimum_required(VERSION 3.21)
project(PackagingProbe VERSION 1.2.3 LANGUAGES CXX)
include(${SPUX_CMAKE_DIR}/SpuxPackaging.cmake)

add_library(Probe MODULE probe.cpp)
set_target_properties(Probe PROPERTIES PREFIX "" SUFFIX ".clap")

spux_deploy_clap(Probe NAME deploy_probe)
spux_package(NAME Probe VERSION ${PROJECT_VERSION} VENDOR spux CLAP_TARGETS Probe
             VST3_PATHS $<TARGET_FILE_DIR:Probe> EXTRA_FILES ${CMAKE_CURRENT_LIST_DIR}/README.txt)
if(PROBE_TOOL)
  set(PROBE_INSTALLER_ARGS TOOL ${PROBE_TOOL})
endif()
spux_add_installer(NAME Probe VERSION ${PROJECT_VERSION} SCRIPT ${CMAKE_CURRENT_LIST_DIR}/probe.iss.in
                   DEPENDS Probe ${PROBE_INSTALLER_ARGS} DEFINES SPUX_COMPANY=Holstebroe)
