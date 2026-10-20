# Packaging helpers (decision P10). They replace the DeployClap.cmake copies in the plugin repositories and
# add a zip through CPack and a hook for an installer script. Include once; the root CMakeLists does.
#
#   spux_deploy_clap(<clap-target> [NAME <custom-target>])
#       Custom target (not part of ALL: a host that has the plugin loaded locks the file) that copies the
#       built .clap into $CLAPTEST.
#
#   spux_package(NAME <name> VERSION <version> CLAP_TARGETS <t>... [VST3_PATHS <path>...]
#                [EXTRA_FILES <file>...] [VENDOR <v>] [GENERATORS ZIP])
#       install() rules and the CPack settings for a release zip, built with `cpack -C Release` (or the
#       `package` target): <name>-<version>-<system>-<arch>.zip holding clap/, vst3/ and the extra files
#       (licence, readme). VST3_PATHS are bundle directories, generator expressions allowed (for
#       clap-wrapper: $<TARGET_BUNDLE_DIR:...>); each is copied into vst3/ under its own name. Call it once, from the top-level CMakeLists, after the
#       plugin targets exist: CPack is one per project.
#
#   spux_add_installer(NAME <name> VERSION <version> SCRIPT <file.iss.in|file.nsi.in> [TARGET <custom-target>]
#                      [DEPENDS <target>...] [DEFINES KEY=VALUE...] [TOOL <path>])
#       The hook for Inno Setup (.iss) or NSIS (.nsi). The plugin owns the script. It is processed twice:
#       @VAR@ is replaced (SPUX_NAME, SPUX_VERSION, SPUX_OUTPUT_DIR and the DEFINES), then generator
#       expressions such as $<TARGET_FILE:plugin> are resolved per configuration. The custom target
#       (default `installer`) runs ISCC or makensis on the result and passes the same values as /D defines.
#       When the tool is missing the target fails with a message and configuring still works.
#       Set SPUX_INSTALLER_TOOL (cache) or TOOL to pick the program explicitly.
set_property(GLOBAL PROPERTY SPUX_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(spux_deploy_clap target)
  cmake_parse_arguments(A "" "NAME" "" ${ARGN})
  if(NOT A_NAME)
    set(A_NAME deploy_${target})
  endif()
  get_property(dir GLOBAL PROPERTY SPUX_CMAKE_DIR)
  add_custom_target(${A_NAME}
    COMMAND ${CMAKE_COMMAND} -DCLAP_FILE=$<TARGET_FILE:${target}> -P ${dir}/SpuxDeployClap.cmake
    COMMENT "Deploying $<TARGET_FILE_NAME:${target}> to $CLAPTEST"
    VERBATIM)
  add_dependencies(${A_NAME} ${target})
endfunction()

function(spux_package)
  cmake_parse_arguments(A "" "NAME;VERSION;VENDOR" "CLAP_TARGETS;VST3_PATHS;EXTRA_FILES;GENERATORS" ${ARGN})
  if(NOT A_NAME OR NOT A_VERSION OR NOT A_CLAP_TARGETS)
    message(FATAL_ERROR "spux_package: NAME, VERSION and CLAP_TARGETS are required")
  endif()
  if(NOT A_GENERATORS)
    set(A_GENERATORS ZIP)
  endif()
  set(component spux_${A_NAME})
  foreach(t ${A_CLAP_TARGETS})
    install(PROGRAMS $<TARGET_FILE:${t}> DESTINATION clap COMPONENT ${component})
  endforeach()
  foreach(p ${A_VST3_PATHS})
    install(DIRECTORY ${p} DESTINATION vst3 COMPONENT ${component} USE_SOURCE_PERMISSIONS)
  endforeach()
  foreach(f ${A_EXTRA_FILES})
    install(FILES ${f} DESTINATION . COMPONENT ${component})
  endforeach()

  string(TOLOWER "${CMAKE_SYSTEM_NAME}" system)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" arch)
  if(arch STREQUAL "amd64")
    set(arch x86_64)
  endif()
  set(CPACK_PACKAGE_NAME "${A_NAME}")
  set(CPACK_PACKAGE_VERSION "${A_VERSION}")
  if(A_VENDOR)
    set(CPACK_PACKAGE_VENDOR "${A_VENDOR}")
  endif()
  set(CPACK_PACKAGE_FILE_NAME "${A_NAME}-${A_VERSION}-${system}-${arch}")
  set(CPACK_GENERATOR "${A_GENERATORS}")
  set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)
  include(CPack)
endfunction()

function(spux_add_installer)
  cmake_parse_arguments(A "" "NAME;VERSION;SCRIPT;TARGET;TOOL" "DEPENDS;DEFINES" ${ARGN})
  if(NOT A_NAME OR NOT A_VERSION OR NOT A_SCRIPT)
    message(FATAL_ERROR "spux_add_installer: NAME, VERSION and SCRIPT are required")
  endif()
  if(NOT A_TARGET)
    set(A_TARGET installer)
  endif()
  get_filename_component(ext "${A_SCRIPT}" NAME)
  string(REGEX REPLACE "\\.in$" "" scriptName "${ext}")
  get_filename_component(scriptExt "${scriptName}" LAST_EXT)
  string(TOLOWER "${scriptExt}" scriptExt)
  if(scriptExt STREQUAL ".iss")
    set(toolNames ISCC iscc)
    set(toolHints "C:/Program Files (x86)/Inno Setup 6" "C:/Program Files/Inno Setup 6")
    set(args /Q)
  elseif(scriptExt STREQUAL ".nsi")
    set(toolNames makensis)
    set(toolHints "C:/Program Files (x86)/NSIS" "C:/Program Files/NSIS")
    set(args /V2)
  else()
    message(FATAL_ERROR "spux_add_installer: SCRIPT must be an .iss (Inno Setup) or .nsi (NSIS) file, optionally with .in")
  endif()

  set(tool "${A_TOOL}")
  if(NOT tool AND SPUX_INSTALLER_TOOL)
    set(tool "${SPUX_INSTALLER_TOOL}")
  endif()
  if(NOT tool)
    find_program(_spux_installer_tool_${scriptExt} NAMES ${toolNames} HINTS ${toolHints})
    set(tool "${_spux_installer_tool_${scriptExt}}")
  endif()

  if(tool AND NOT EXISTS "${tool}")
    message(STATUS "spux_add_installer: ${tool} does not exist")
    set(tool "")
  endif()

  set(outDir "${CMAKE_BINARY_DIR}/installer")
  set(SPUX_NAME "${A_NAME}")
  set(SPUX_VERSION "${A_VERSION}")
  set(SPUX_OUTPUT_DIR "${outDir}")
  set(defines SPUX_NAME=${A_NAME} SPUX_VERSION=${A_VERSION} SPUX_OUTPUT_DIR=${outDir})
  foreach(d ${A_DEFINES})
    string(FIND "${d}" "=" eq)
    string(SUBSTRING "${d}" 0 ${eq} key)
    math(EXPR valueStart "${eq} + 1")
    string(SUBSTRING "${d}" ${valueStart} -1 value)
    set(${key} "${value}")
    list(APPEND defines ${d})
  endforeach()

  set(stage "${CMAKE_CURRENT_BINARY_DIR}/spux_installer_${A_NAME}${scriptExt}.stage")
  configure_file("${A_SCRIPT}" "${stage}" @ONLY)
  set(script "${outDir}/$<CONFIG>/${A_NAME}${scriptExt}")
  file(GENERATE OUTPUT "${script}" INPUT "${stage}")

  if(tool)
    set(toolArgs ${args})
    foreach(d ${defines})
      list(APPEND toolArgs /D${d})
    endforeach()
    add_custom_target(${A_TARGET}
      COMMAND ${CMAKE_COMMAND} -E make_directory "${outDir}"
      COMMAND "${tool}" ${toolArgs} "${script}"
      COMMENT "Building the ${A_NAME} installer with ${tool}"
      VERBATIM)
  else()
    add_custom_target(${A_TARGET}
      COMMAND ${CMAKE_COMMAND} -E echo "spux_add_installer: no installer compiler (${toolNames}) found or the given one does not exist; install it or set SPUX_INSTALLER_TOOL"
      COMMAND ${CMAKE_COMMAND} -E false
      VERBATIM)
  endif()
  if(A_DEPENDS)
    add_dependencies(${A_TARGET} ${A_DEPENDS})
  endif()
endfunction()
