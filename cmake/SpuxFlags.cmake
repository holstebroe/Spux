# Compiler and linker flags shared by every spux target and by the plugins that call
# spux_target_flags(<target>) (DESIGN_GUIDELINES sections 2 and 4).
#
# Reproducibility contract (R3): no fast-math, no floating-point contraction.
# Only-what-is-used (section 4): per-function and per-data sections plus linker garbage collection,
# and hidden visibility.
set(SPUX_SANITIZE "" CACHE STRING "Sanitizers for spux targets, e.g. address,undefined")

function(spux_target_flags target)
  get_target_property(type ${target} TYPE)
  if(type STREQUAL "INTERFACE_LIBRARY")
    # Header-only: the consuming target applies the flags with its own spux_target_flags call.
    return()
  endif()

  if(MSVC)
    # /Zc:__cplusplus: report the real __cplusplus. C5285: newer MSVC rejects doctest's std::tuple
    # specialization (vendored header, not ours).
    target_compile_options(${target} PRIVATE /W4 /WX /fp:precise /permissive- /utf-8 /Zc:__cplusplus /Gy /wd5285)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math
                                             -ffunction-sections -fdata-sections)
  endif()

  # Hidden visibility [L13]; a plugin exports its entry point explicitly (CLAP_EXPORT).
  set_target_properties(${target} PROPERTIES CXX_VISIBILITY_PRESET hidden C_VISIBILITY_PRESET hidden
                                              VISIBILITY_INLINES_HIDDEN ON)

  # Linker garbage collection only applies where something is linked.
  if(type STREQUAL "MODULE_LIBRARY" OR type STREQUAL "SHARED_LIBRARY" OR type STREQUAL "EXECUTABLE")
    if(MSVC)
      target_link_options(${target} PRIVATE /OPT:REF /OPT:ICF)
    elseif(APPLE)
      target_link_options(${target} PRIVATE -Wl,-dead_strip)
    else()
      target_link_options(${target} PRIVATE -Wl,--gc-sections)
    endif()
  endif()

  if(SPUX_SANITIZE AND NOT MSVC)
    target_compile_options(${target} PRIVATE -fsanitize=${SPUX_SANITIZE} -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=${SPUX_SANITIZE})
  endif()
endfunction()
