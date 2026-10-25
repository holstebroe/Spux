# Spux

Signal Processing & User eXperience: a lean C++20 toolkit for CLAP/VST3 plugins: DSP, parameters, state
and Retro/Classic/Modern UIs.

spux is not a framework. A plugin picks the parts it wants; only those are compiled and linked, so a
Retro build stays small and a headless build stays fast. It holds the DSP building blocks, parameter and
state plumbing, CLAP glue (VST3 through clap-wrapper), windowing and the editor UI in three flavors that
the author's plugins share.

spux is unrelated to SPUC ("Signal Processing using C++", a GPL DSP library).

## Status

Design phase. The CMake skeleton, flags and CI exist (migration step 1); the Classic drawing code (step 2), the native window and GL context (step 3), the testing, MIDI and packaging utilities (step 3a), the core and DSP building blocks (step 4), the parameters, state and CLAP glue (step 5) the immediate-mode UI core (step 6, its acceptance gate in `docs/UI_GATE.md`) the Modern flavor (step 7: GPU and software renderers, `ModernStyle`, generic views, the editor's GPU path) and the Classic flavor (step 7: `ShadedRenderer`, `ClassicStyle`) are extracted; `docs/WIDGETS.md` shows every control per flavor. Start with:

- `docs/DESIGN_GUIDELINES.md`: how spux is built and how plugins use it (the rules).
- `docs/DECISIONS.md`: every decision, with status and reason.
- `docs/MIGRATION.md`: what moves in from which plugin, and in what order.
- `CLAUDE.md`: the short version for AI agents.

## Modules (CMake targets)

Header-only parts are compiled only where a plugin uses them. Every other part is a separate small
target; a plugin links the targets it lists and nothing else.

| Target | Contents | Depends on |
| --- | --- | --- |
| `spux_core` (header-only) | Deterministic math, constants, PCG32, control tick, smoothing policies, denormal guard, the telemetry rings `TelemetryRing<Frame>` and `SampleRing` (built) | nothing |
| `spux_dsp` (header-only) | DSP building blocks: one-pole, SVF, biquad, DC block, half-band and oversampler, BLEP (and the pt2 BSD-3 table), delay lines, saturators with ADAA and solvers, noise, drift (built); tuning helpers (later) | `spux_core` |
| `spux_params` (header-only) | `ParamInfo`, laws and host mapping, X-macro helpers, `ParamTable` (id and key lookup, checks), value text and parsing, atomic `ParamStore`, `EditQueue` with a gesture reserve (built) | `spux_core` |
| `spux_state` (header-only) | Chunked state container (magic per plugin, version, CRC-32, unknown and damaged chunks skipped), byte reader and writer, parameter chunk, migration hook, text preset files (key=value, locale-free numbers) (built) | `spux_core` |
| `spux_clap` (header-only) | Thin CLAP glue: `spux::clap::Plugin<Derived>` (CRTP) offers the params, state, audio and note ports, GUI and timer extensions the plugin class implements; helpers for parameter info, events, the edit queue and streams; a one-class factory (built) | `spux_params`, `spux_state`, clap |
| `spux_midi` (header-only) | Standard MIDI file reader (raw events, tempo map, timed and paired notes) and writer (tracks with tick priorities) | `spux_core` |
| `spux_presets` (header-only) | Factory presets as a folder of text files: `PresetLibrary` (the folder in a development build, otherwise the baked table; save back to a file, add a preset), `tools/bake_presets.py`, CMake helpers (`SPUX_PRESETS_DEV`) | `spux_core` |
| `spux_platform` | Native child window (Win32, X11, later Cocoa) with input events and a tick timer | OS only |
| `spux_ui_core` | Immediate-mode `Gui` (`Gui<Style>` over `GuiCore`), `IRenderer`, the CPU layer cache, painter interfaces and `Theme`, `EditorHost` with optional extensions, layout helpers, the editor shell (CPU renderers, or a `FrameSurface` for a GPU), telemetry consumers (built) | `spux_platform`, `spux_params` |
| `spux_ui_retro` | The Retro flavor (built): `PixelRenderer` (whole pixels, bitmap text, pixel-art effects), `RetroStyle` with its `RetroConfig` (`silver()`, `tracker()`, `forge()`), the bitmap fonts (5x7, 7-pixel panel, pt2 8x8). Static, one object per part and per look | `spux_ui_core` |
| `spux_ui_classic` | Classic style (`ClassicStyle`, `ClassicConfig`), the supersampled CPU renderer with shading helpers (`ShadedRenderer`), the drawing code (`Graphics`, `ClassicDraw`, `FileDialog`), atlas fonts | `spux_ui_core` |
| `spux_ui_modern` | The Modern flavor (built): the GL function table and context (`spux::gl`), `GpuRenderer` (OpenGL 3.3, bloom chain, embedded shaders), the Inter SDF font atlas, `SoftRenderer` (the same style on the CPU), `ModernStyle` with its `ModernConfig`, generic views (spectrum, traces, stereo meter, curve editor), `ModernSurface` and `modernBackend()` (the editor's GPU path with the software fallback). Static, one object per part | `spux_ui_core` |
| `spux_presetbrowser` | Optional shared preset browser | `spux_ui_core`, `spux_state` |
| `spux_testing` (header-only) | WAV read and write, BS.1770 loudness, analysis metrics (filters, FFT, band levels, Schroeder decay), render-safety check, FNV golden hashes and golden files, reference-render null test | `spux_core`, doctest |
| `spux_testing_allocguard` | Replaces the global `operator new` to catch allocations on the audio thread. Test executables only | nothing |
| `spux_testing_clap` | CLAP smoke host: loads a built `.clap` like a DAW and runs the shared checks. Test executables only (`spux_clap_smoke` is the command line tool) | clap, X11 on Linux |

Commercial-only code (license checks and similar) lives in the private `spux-commercial` library, which
depends on spux, never the other way round.

## Testing and packaging helpers

- **Null test.** `runNullTest(name, rendered, options)` compares a render with the float WAV stored in the plugin repository and
  reports the peak and RMS of the difference per case; the threshold is `NullOptions::peakThresholdDb` (-90 by default,
  `-infinity` for bit-exact). Optional sound tolerances, `maxSpectralDb` and `maxEnvelopeDb`, gate on what is heard instead
  (Acidus: 0.08 and 0.12 dB, with the peak as a report). `SPUX_UPDATE_REFERENCES=1` regenerates the references (a case without one fails). See `spux/testing/NullTest.hpp`.
- **Golden hashes.** `Fnv1a64`, `hashFloats`, `GoldenFile` (one file per platform and compiler, `SPUX_UPDATE_GOLDEN=1` to record).
- **Smoke host.** `spux_add_clap_smoke_test(name plugin-target ARGS ...)` registers a ctest entry; the same checks are a library
  call (`runClapSmoke`) with an `extra` callback for plugin-specific ones.
- **Packaging** (`cmake/SpuxPackaging.cmake`, included by spux): `spux_deploy_clap(target)` copies the `.clap` to `$CLAPTEST`;
  `spux_package(NAME .. VERSION .. CLAP_TARGETS ..)` sets up a release zip with CPack (`cpack -C Release`); `spux_add_installer(..)`
  runs Inno Setup or NSIS on the plugin's own script.

## Using spux in a plugin

spux is a git submodule at `extern/spux`, pinned to a release tag. spux itself owns the versions of
clap, clap-wrapper, doctest and stb_truetype.

```cmake
set(MYPLUGIN_UI "RETRO" CACHE STRING "RETRO, CLASSIC, MODERN or NONE")
set(SPUX_UI_FLAVOR ${MYPLUGIN_UI})      # before adding spux: Retro builds spux's UI code for size
if(SPUX_SOURCE_DIR)                      # -DSPUX_SOURCE_DIR=/path/to/local/spux while developing
  add_subdirectory(${SPUX_SOURCE_DIR} spux)
else()
  add_subdirectory(extern/spux)
endif()
target_link_libraries(myplugin_core PRIVATE spux_dsp spux_params spux_state)
target_link_libraries(myplugin PRIVATE spux_clap spux_ui_${MYPLUGIN_UI_LOWER})
spux_target_flags(myplugin)              # determinism and warning flags, hidden visibility
```

## Building spux itself

```
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target spux_size_canary_report
```

Options: `SPUX_BUILD_TESTS`, `SPUX_BUILD_CANARY`, `SPUX_BUILD_SMOKE_TOOL`, `SPUX_BUILD_BENCH` (all on only in a top-level build; `build/spux_bench` times spux math against libm, results in `docs/BENCH.md`), `SPUX_BUILD_VST3`
(makes clap-wrapper's `make_clapfirst_plugins` available), `SPUX_SANITIZE=address,undefined`, `SPUX_UI_FLAVOR` and `SPUX_UI_OPTIMIZE` (`AUTO`: spux's UI code for size in a
Retro build, for speed otherwise; guidelines 4.9).

## License

MIT. Third-party code inside spux keeps its own license in its own folder (the BSD-3 BLEP table from
pt2-clone in `include/spux/dsp/pt2/`, the OFL font Inter and stb_truetype in `third_party/`); `THIRD_PARTY_LICENSES.md` lists what a
shipped plugin must include.
