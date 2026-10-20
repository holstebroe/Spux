# Script mode: cmake -DCLAP_FILE=<built .clap> -P SpuxDeployClap.cmake
# Copies the built plugin into the folder named by the CLAPTEST environment variable, which is read when
# the target runs, not when CMake configured (so a changed variable needs no reconfigure). Replaces the
# DeployClap.cmake copies in the plugin repositories; `spux_deploy_clap()` registers it.
if(NOT CLAP_FILE)
  message(FATAL_ERROR "DeployClap: CLAP_FILE is not set")
endif()
if(NOT DEFINED ENV{CLAPTEST} OR "$ENV{CLAPTEST}" STREQUAL "")
  message(FATAL_ERROR "DeployClap: set the CLAPTEST environment variable to the folder the .clap should be copied to")
endif()

file(MAKE_DIRECTORY "$ENV{CLAPTEST}")
get_filename_component(_name "${CLAP_FILE}" NAME)
if(IS_DIRECTORY "${CLAP_FILE}")
  # A macOS bundle is a directory.
  file(COPY "${CLAP_FILE}" DESTINATION "$ENV{CLAPTEST}")
  set(_result 0)
else()
  file(COPY_FILE "${CLAP_FILE}" "$ENV{CLAPTEST}/${_name}" ONLY_IF_DIFFERENT RESULT _result)
endif()
if(NOT _result STREQUAL "0")
  message(FATAL_ERROR "DeployClap: could not copy ${_name} to $ENV{CLAPTEST} (${_result}). Is a host holding the file open?")
endif()
message(STATUS "Deployed ${_name} to $ENV{CLAPTEST}")
