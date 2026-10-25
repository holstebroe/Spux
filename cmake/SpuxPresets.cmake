# Factory preset helpers (spux/presets/PresetLibrary.hpp).
#
#   option SPUX_PRESETS_DEV (OFF): a development build reads and writes the preset folders instead of serving the
#       compiled tables. Release builds leave it off.
#
#   spux_presets_dev_folder(<source file> <DEFINE> <folder>)
#       With SPUX_PRESETS_DEV, defines <DEFINE> as the quoted folder path for the one source file that opens the
#       library, which passes it to PresetLibrary (an empty folder otherwise).
#
#   spux_presets_bake(<name> FOLDER <dir> OUTPUT <cpp> SYMBOL <symbol> [NAMESPACE <a::b>] [EXTENSION <.txt>])
#       A target <name> that bakes the folder into the checked-in <cpp>, and the ctest <name>_check that fails when
#       <cpp> is not what baking gives (so a preset edited without baking is caught). Needs Python 3.
option(SPUX_PRESETS_DEV "Read and write the factory preset folders instead of the compiled tables" OFF)
set(SPUX_PRESETS_BAKE_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/../tools/bake_presets.py" CACHE INTERNAL "")

function(spux_presets_dev_folder source define folder)
  if(SPUX_PRESETS_DEV)
    set_property(SOURCE ${source} APPEND PROPERTY COMPILE_DEFINITIONS "${define}=\"${folder}\"")
  endif()
endfunction()

function(spux_presets_bake name)
  cmake_parse_arguments(P "" "FOLDER;OUTPUT;SYMBOL;NAMESPACE;EXTENSION" "" ${ARGN})
  if(NOT P_EXTENSION)
    set(P_EXTENSION ".txt")
  endif()
  find_package(Python3 COMPONENTS Interpreter)
  if(NOT Python3_FOUND)
    return()
  endif()
  set(args ${P_FOLDER} ${P_OUTPUT} --symbol ${P_SYMBOL} --extension ${P_EXTENSION})
  if(P_NAMESPACE)
    list(APPEND args --namespace ${P_NAMESPACE})
  endif()
  add_custom_target(${name} COMMAND ${Python3_EXECUTABLE} ${SPUX_PRESETS_BAKE_SCRIPT} ${args}
                    COMMENT "Baking ${P_FOLDER} into ${P_OUTPUT}" VERBATIM)
  add_test(NAME ${name}_check COMMAND ${Python3_EXECUTABLE} ${SPUX_PRESETS_BAKE_SCRIPT} ${args} --check)
endfunction()
