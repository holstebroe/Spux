# Spux

Signal Processing & User eXperience: a lean C++20 toolkit for CLAP/VST3 plugins: DSP, parameters, state
and Retro/Classic/Modern UIs.

spux is not a framework. A plugin picks the parts it wants; only those are compiled and linked, so a
Retro build stays small and a headless build stays fast. It holds the DSP building blocks, parameter and
state plumbing, CLAP glue (VST3 through clap-wrapper), windowing and the editor UI in three flavors that
the author's plugins share.

spux is unrelated to SPUC ("Signal Processing using C++", a GPL DSP library).

## Status

Design phase. Nothing is extracted yet. Start with:

- `docs/DESIGN_GUIDELINES.md`: how spux is built and how plugins use it (the rules).
- `docs/DECISIONS.md`: every decision, with status and reason.
- `docs/MIGRATION.md`: what moves in from which plugin, and in what order.
- `CLAUDE.md`: the short version for AI agents.

## Modules (CMake targets)

Header-only parts are compiled only where a plugin uses them. Every other part is a separate small
target; a plugin links the targets it lists and nothing else.

| Target | Contents | Depends on |
| --- | --- | --- |
| `spux_core` (header-only) | Deterministic math, constants, PCG32, control tick, smoothing policies, denormal guard, SPSC rings | nothing |
| `spux_dsp` (header-only) | DSP building blocks: one-pole, SVF, biquad, DC block, half-band and oversampler, BLEP, delay lines, saturators with ADAA, noise, drift, tuning helpers | `spux_core` |
| `spux_params` | `ParamInfo`, laws, X-macro helpers, value text, key lookup, atomic store, edit queue | `spux_core` |
| `spux_state` | Chunked state container (magic per plugin, version, CRC, unknown chunks skipped) | `spux_core` |
| `spux_clap` | Thin CLAP glue (traits/CRTP): params, state, ports, GUI, timer | `spux_params`, `spux_state`, clap |
| `spux_midi` | Standard MIDI file read and write | `spux_core` |
| `spux_platform` | Native child window (Win32, X11, later Cocoa), timer, file dialog | OS only |
| `spux_ui_core` | Immediate-mode `Gui`, `IRenderer`, layout helpers, editor shell, telemetry views | `spux_platform`, `spux_params` |
| `spux_ui_retro` | Retro style and the pixel renderer, bitmap fonts | `spux_ui_core` |
| `spux_ui_classic` | Classic style, the supersampled CPU renderer with shading helpers, atlas fonts | `spux_ui_core` |
| `spux_ui_modern` | Modern style, GPU renderer (OpenGL 3.3), SDF font atlas, software fallback | `spux_ui_core` |
| `spux_presetbrowser` | Optional shared preset browser | `spux_ui_core`, `spux_state` |
| `spux_testing` | Allocation guard, reference-render null test, golden hashes, WAV I/O, loudness, render-safety check, CLAP smoke host | `spux_core` |

Commercial-only code (licence checks and similar) lives in the private `spux-commercial` library, which
depends on spux, never the other way round.

## Using spux in a plugin

spux is a git submodule at `extern/spux`, pinned to a release tag. spux itself owns the versions of
clap, clap-wrapper and doctest.

```cmake
add_subdirectory(extern/spux)            # or -DSPUX_SOURCE_DIR=/path/to/local/spux while developing
set(MYPLUGIN_UI "RETRO" CACHE STRING "RETRO, CLASSIC, MODERN or NONE")
target_link_libraries(myplugin_core PRIVATE spux_dsp spux_params spux_state)
target_link_libraries(myplugin PRIVATE spux_clap spux_ui_${MYPLUGIN_UI_LOWER})
spux_target_flags(myplugin)              # determinism and warning flags, hidden visibility
```

## Licence

MIT. Third-party code inside spux keeps its own licence in its own folder (for example the BSD-3 BLEP
table from pt2-clone, the OFL font Inter); `THIRD_PARTY_LICENSES.md` (added with the first third-party code) lists what a shipped plugin must
include.
