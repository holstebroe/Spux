# spux decision log

Status: **Decided** (by the author), **Proposed** (recommended, not yet confirmed), **Open**,
**Deferred**. Change a decision by editing its row and adding a line to the history at the end.

## Library and repository

| # | Decision | Status | Notes |
| --- | --- | --- | --- |
| L1 | Name **spux** (Signal Processing & User eXperience). Namespace `spux`, with an inline version namespace (`spux::v1`). | Decided | Unrelated to SPUC; say so in the README. Pronunciation still to settle. |
| L2 | **C++20** baseline. The C++17 plugins move up. | Decided | |
| L3 | **One repository, many small CMake targets** (no single big library). | Decided | A split into DSP and UI repos was rejected: the change rules (S1) protect the sound instead. |
| L4 | Plugins consume spux as a **git submodule pinned to a release tag**, with `SPUX_SOURCE_DIR` to point at a local checkout while developing. | Decided | Same pattern as Tearwash today. |
| L5 | **Semver tags.** Major version for API breaks. | Decided | |
| L6 | spux **owns the third-party versions** (clap, clap-wrapper, doctest, stb_truetype) as its own submodules. Plugins drop theirs. | Decided | |
| L7 | clap-wrapper: **the author's fork** (`vst3-midi-out`) for now; switch to upstream once the changes are accepted there. | Decided | One place to change, because of L6. |
| L8 | **MIT** license. Third-party code keeps its license in its own folder. | Decided | Check the VST3 SDK license before the first commercial release. |
| L9 | Commercial-only code goes in a **private `spux-commercial`** library. Public plugins build without it (stub). | Decided | |
| L10 | Design guidelines and a **CLAUDE.md** live in spux. Each plugin's CLAUDE.md gets a short "using spux" section. | Decided | |
| L11 | Code style: `PascalCase` types and files, `camelCase` functions, `kConstants`, `member_`, lower-case namespaces, `.hpp`/`.cpp`, `#pragma once`, `.clang-format` (LLVM base, 4 spaces, 120 columns) checked in CI. | Proposed | Matches most existing repos. Lumeoid and Paulascape use `snake_case` files today. |
| L12 | **Exceptions and RTTI allowed** outside the audio thread. | Decided | The audio thread never throws (R1). |
| L13 | **Hidden symbol visibility** (`-fvisibility=hidden`) plus the inline version namespace. | Decided | Two plugins with different spux versions can share one host process. |
| L14 | Tests use **doctest**. | Decided | |
| L15 | CI: keep the current GitHub Actions setup; spux gets the same. | Decided | |
| L16 | **Instances never affect each other's output.** Shared state between the instances of one plugin only as immutable data, memo caches of pure functions (keyed on every input, filled off the audio thread, read without blocking) or per-module OS registrations; prefer a context owned by the plugin's entry. Separation between plugins and spux versions is L13's job. | Decided | Replaces "no process-global mutable state" (2026-10-20). Guidelines 1.6. |

## Sound compatibility

| # | Decision | Status | Notes |
| --- | --- | --- | --- |
| S1 | **Change rules instead of freezing.** Bug fixes, optimizations and inaudible rounding changes are allowed in existing code, and so are new methods. A change of character (curve, tuning, law) is a new class, policy or option; the old one stays the default. | Decided | See the guidelines, "Sound compatibility". |
| S2 | The change rules apply to a plugin from its **1.0 release**. Before that, duplicates are merged freely. | Decided | |
| S3 | **Bit-identical within one build** (any block size, every re-render). Across spux versions, inaudible differences are allowed. | Decided | |
| S4 | **libm is the default in the audio path.** spux math is opt-in: where `spux_bench` shows a gain that matters (`fastTanhF` in a hot loop), where constexpr is needed, or where identical bits on every platform are wanted. | Decided | Changed 2026-10-20 (was "no libm in the audio path, where spux is at least as fast"): libm's last-bit differences between platforms are as inaudible as spux's against libm, and libm costs a plugin no size (a shared library). |
| S5 | **Reference renders** stored in each plugin repo; a null test compares against them with a single checkout. | Decided | Threshold: proposed -90 dBFS peak of the difference; where rounding alone exceeds it, the sound tolerances (spectrum, envelope) gate instead (2026-10-20). LFS or plain git: Open. |
| S6 | **Acidus is strict.** Its calibration must survive refactoring: every spux update in Acidus runs its reference conformance test (the whole report) and its null test within sound tolerances, and reports every difference per patch before it is accepted. Other plugins may change more freely. | Decided | Tolerances 0.08 dB spectrum, 0.12 dB envelope (2026-10-20; was -110 dBFS peak, which on Acidus's ladder meant identical bits). |
| S7 | Every plugin state carries a **version field**, so a later converter can rescue old data. | Decided | |
| S8 | **Sample type is a template parameter** for building blocks; `float` audio and `double` coefficients by default. | Decided | |
| S9 | **Control tick is a template parameter**, default 64 samples. A plugin may use none. | Decided | |
| S10 | **Smoothing as policies** (`NoSmoothing`, `LinearRamp`, `OnePole`, `Slew`, ...), chosen per parameter by the plugin. A shipped policy never changes its curve. | Decided | Acidus does not smooth parameters today and stays as it is. |
| S11 | **Sample rates and oversampling are chosen per plugin.** | Decided | |

## Plugin infrastructure

| # | Decision | Status | Notes |
| --- | --- | --- | --- |
| P1 | **Thin CLAP glue** (traits/CRTP), not a framework. | Decided | |
| P2 | **Parameters from an X-macro `.def` file.** Laws live in spux and follow S1. | Decided | The host stores normalized values; a changed law changes old projects. |
| P3 | **One chunked state container**, magic chosen per plugin, version and CRC, unknown chunks skipped. | Decided | Lumeoid `LMN1` is the starting point. |
| P4 | Presets: **compiled-in factory presets and user files**. User files are **human-readable text keyed by parameter key** (spux's text preset format); factory presets take whatever compact compiled form suits the plugin. | Decided | Changed 2026-10-20 (was: both go through the state container). |
| P5 | Formats: **CLAP and VST3** through clap-wrapper. | Decided | |
| P6 | **Shared helpers are opt-in**: modulation matrix, voice allocation, MPE, tuning, transport sync, poly modulation, preset browser. The plugin decides; unused parts are never compiled or linked. | Decided | |
| P7 | **Audio→GUI telemetry**: a shared lock-free ring templated on the frame type, switched off while the editor is hidden. | Decided | From Plumeflow `Telemetry`. |
| P8 | **Real-time asserts** (allocation guard, thread checks) in debug and test builds only. | Decided | |
| P9 | The **state versioning rule** belongs to each plugin; spux gives the version field and a migration hook. | Decided | |
| P10 | Packaging: **installers and zips**; spux provides CMake helpers and replaces the copies of `DeployClap.cmake`. | Decided | Inno Setup or NSIS on Windows. |
| P11 | **CPU budget** measured in Release builds. CI fails on a regression over a stored baseline; absolute targets per plugin. | Decided | Threshold: proposed 10 to 15 %. |
| P12 | **Plugin IDs** (CLAP reverse-DNS ids, VST3 class ids). | Deferred | Waits for the brand name. They can never change after release. |

## User interface

| # | Decision | Status | Notes |
| --- | --- | --- | --- |
| U1 | Three flavors: **Retro** (pixel), **Classic** (code-shaded hardware look), **Modern** (GPU). | Decided | The existing `modern/` folders in Acidus, Tannhauser and Tearwash are Classic and are renamed. |
| U2 | **One flavor per build**, chosen by a CMake option (`<PLUGIN>_UI=RETRO|CLASSIC|MODERN|NONE`). | Decided | |
| U3 | **`UI=NONE`** builds no UI at all (fast DSP work and CI). | Decided | |
| U4 | **The plugin decides which flavors it supports.** spux forces none; a missing painter is a compile error only in a plugin that selects that flavor. | Decided | Replaces "Retro is always available". |
| U5 | **Modern falls back to the Modern style on the software renderer**, not to Classic. | Decided | |
| U6 | CI builds each plugin's native flavor on every push and the full matrix nightly. | Decided | |
| U7 | **Immediate-mode widget API with retained-style caches.** The retained widget tree behind the same painter interfaces was the fallback and is not needed. | Decided | 2026-10-20 after the acceptance gate (docs/UI_GATE.md): simpler page code and faster frames than the retained panels; the size cost (Acidus Retro about +33 % with U16, Tannhauser Classic +7.6 %) accepted by the author. |
| U8 | **Painters per widget and flavor**; a flavor's look is a `Theme` **data struct** (colors, metrics, fonts). | Decided | |
| U9 | **Editing helpers** in the core: value entry, Tab focus, tooltips, popups, double-click reset, fine adjust. | Decided | Plumeflow's `Gui` is the baseline. Cascading submenus (Tannhauser's preset menu) go into spux's own popup, the simple option: a plugin cannot draw its own overlays (2026-10-20). |
| U10 | **Shared preset browser** as an optional target; plugins may write their own. | Decided | |
| U11 | Scaling: Retro whole steps; Classic in steps up to a cap; Modern **free resize at a fixed aspect ratio**. | Decided | |
| U12 | Fonts: Retro bitmap; Classic pre-rasterized atlas; Modern **SDF atlas baked at editor open** from an embedded TTF subset (stb_truetype). | Decided | Smaller binary than a pre-baked atlas (about 34 KB TTF vs 1 MB atlas). |
| U13 | GPU: **OpenGL 3.3 core** with a **runtime-loaded function table** (no glad), one context per editor, nothing process-global. | Decided | As built in Plumeflow. Metal or WebGPU later behind the same renderer. |
| U14 | Platforms: **Windows first**, Linux in CI, macOS later (Cocoa window and CGL context still to write). | Decided | |
| U15 | Accessibility (screen readers). | Open | Not needed now. Immediate mode can emit a widget list per frame if it becomes needed. |
| U16 | **UI code optimization follows the plugin's kind**: a simple Retro plugin builds its UI (spux's and its own) for size, a plugin large in concept and number of controls (Classic, Modern) for speed. `SPUX_UI_OPTIMIZE` (`AUTO` by flavor, `SIZE`, `SPEED`). | Decided | 2026-10-20, after the gate (docs/UI_GATE.md): `-Os` saves about 60 KB on Acidus for about 20 % more UI instructions, still below the retained version. Guidelines 4.9. |

## Migration

| # | Decision | Status | Notes |
| --- | --- | --- | --- |
| M1 | **Pilots: Tearwash and lexwash.** Then Tannhauser and Acidus, then Plumeflow and Lumeoid. | Decided | Tearwash has the vendored GUI copy; lexwash starts clean. |
| M2 | Whether Gritbaal and Syrebas are migrated or folded into Acidus. | Open | Both are older Acidus forks. |
| M3 | Mimicry uses spux core, DSP and testing parts, never its UI. | Proposed | |

## Open numbers

| Item | Proposal |
| --- | --- |
| Null-test threshold (S5) | Difference below -90 dBFS peak, or -100 dB RMS relative to the signal |
| Acidus threshold (S6) | Decided 2026-10-20: spectrum within 0.08 dB, envelope within 0.12 dB, plus the conformance report |
| CPU regression gate (P11) | Fail above 10 to 15 % over the stored baseline |
| Reference-render storage (S5) | Plain git while small; Git LFS if a plugin's set grows past about 20 MB |

## History

- 2026-10-19: first version, from the analysis of Acidus, Gritbaal, Kikset, Lumeoid, Paulascape,
  Plumeflow, Rodee, Syrebas, Tannhauser and Tearwash, and the Plumeflow WP8 GPU editor.
- 2026-10-19: migration step 1 (CMake skeleton, CI). No decision changed. Pins: clap 1.2.10, doctest v2.5.3,
  clap-wrapper = the fork at `vst3-midi-out` (98bed60). L11 stays Proposed; `.clang-format` and the CI check exist now.
  Plugins pin spux to a commit until tags start (L4 applies from the first tag).
- 2026-10-19: migration step 2 (Classic drawing code into `spux_ui_classic`, Tearwash as pilot). No decision
  changed. `modern/` became `classic/` as U1 says; `ModernDraw` became `ClassicDraw`. `spux_ui_classic` is a
  STATIC library and the first module with sources; a plugin on C++17 (Tearwash) is raised to C++20 by it (L2).
- 2026-10-19: migration step 3 (native window and GL context). No decision changed. `spux_platform` and the GL
  parts of `spux_ui_modern` are STATIC libraries now (the guidelines listed the GL context under the
  platform; it lives in `spux_ui_modern`, U13, and the layer table says so). The Win32 window class is
  registered per module under a name made from the module handle, so plugins with different spux versions
  in one host never share a window class (L13). `WindowOptions::keyboard` (default true, so Plumeflow is
  unchanged) lets a window without key input leave the keyboard to the host.
  `SPUX_WITH_X11` (default ON) turns the X11 window and GLX context off for headless Linux builds; windows then report no support.
- 2026-10-19: migration step 3a (testing, MIDI and packaging utilities). No decision changed. P10 is implemented as
  `spux_deploy_clap`, `spux_package` (CPack zip) and `spux_add_installer` (a hook for the plugin's own Inno Setup or NSIS
  script); the installer choice stays with the plugin. S5/S6 numbers are parameters of the null test (default -90 dBFS peak,
  optional RMS relative to the reference); the open numbers table is unchanged. Differences from the merged originals that a
  plugin will meet when it switches: the allocation guard aborts on a violation by default (Lumeoid; Plumeflow counted) and also
  sees nothrow and aligned `new`; `DeployClap` creates the `CLAPTEST` folder when it is missing (Paulascape; three copies
  stopped with an error); 16-bit WAV pads an odd data chunk; the MIDI reader rejects SMPTE time and keeps meta and sysex
  events, and paired notes are keyed by channel and key and a repeated note-on ends the sounding note (Plumeflow keyed by key
  only and dropped the first note). The standard FNV-1a offset basis is used by `Fnv1a64`; the Classic drawing golden was
  made with a basis missing its last digit and keeps it as `kFnvLegacyBasis` (the golden is unchanged).
- 2026-10-20: migration step 4 (`spux_core`, `spux_dsp`). No decision changed. New choices within the guidelines:
  **one half-band design, Lumeoid's 5 + 5 all-pass sections** (-108 dB stopband against -95 dB for Plumeflow's
  4 + 4, about 25 % more CPU; docs/BENCH.md has the measurement and the reasons). The pt2-clone BLEP lives in
  `include/spux/dsp/pt2/` with its BSD-3 license; `THIRD_PARTY_LICENSES.md` exists now. Kernel golden hashes are
  in `tests/kernels/portable.txt` (the blocks call no libm, so GCC and Clang on x64 agree); a toolchain that
  differs gets its own `tests/kernels/<platform>.txt`. Function names that would collide with `<cmath>` under
  `using namespace` were renamed: Plumeflow `exp2`, `tanh` are `exp2F`, `tanhF`; Lumeoid `tanhApprox` is
  `tanhPade76`; Tannhauser `tanhFast` is `tanhPade32`. Differences a plugin meets when it switches: a
  default-constructed `Pcg32` has the PCG reference state (Lumeoid's), not Plumeflow's seed(0, 1); Lumeoid's
  `reseed` is `seed` and `Oversampler::prepare` is `setFactor`; Plumeflow's `DelayLine::push` is `write`, and
  its fractional reads interpolate from the newer sample (last-bit differences); `exp2F` maps NaN to 2^-126
  instead of undefined behavior; the OU setup uses expD (1e-11 cents); Plumeflow's `DenormalGuard` becomes
  Lumeoid's, which also sets round-to-nearest and masks exceptions; the BLEP tables are built with spux math
  (bit-identical to Plumeflow's tables here). Lumeoid's `sinD` comment claimed exactness to double precision;
  measured 6e-10 (its test checked 1e-9), now documented. S4 finding, open: `exp2F` and `dbToGainF` are slower
  than glibc's `exp2f` (0.3x and 0.6x, latency of the Horner chain); the other float functions are as fast or
  faster, and the double set is slower than libm by design (control rate only).
- 2026-10-20: Acidus onto spux DSP (session 6, S6 strict gate). No decision changed. Findings for S4/S6: the -110 dBFS
  threshold admits no change to the Acidus ladder's tanh except a bit-identical one (a last-bit difference moves its
  output by -70 to -90 dBFS from the first note, so `tanhF` would save about 18 % of the engine's CPU but cannot pass;
  the author decides). The Acidus conformance test fails ten checks by design, so its gate is the full report compared
  with a stored copy, not its exit code. `tanhF(0)` is -6e-8 and `tanhF` is not odd (within its documented 1e-6).
  `expD`/`powD` rounded to float differ from glibc's `expf`/`powf` by one ulp for about 0.03 % / 0.06 % of arguments.
- 2026-10-20: **L16 added** (by the author): CLAUDE.md rule 8 and guidelines principle 6 said "no process-global mutable
  state", which also forbade harmless sharing (immutable tables, memo caches of pure functions such as Acidus's
  critical-gain tables) while the separation of plugins and spux versions comes from L13. The rule now names what it
  protects: no instance can change what another computes or make it wait or allocate. Allowed shared forms: immutable
  data, memo caches keyed on every input (filled off the audio thread, read without blocking, recomputed on a miss),
  per-module OS registrations; an entry-owned shared context is preferred over function-local statics. U13's "nothing
  process-global" (the GL function table and context) is an instance of it and stays.
- 2026-10-20: **S4, S5 and S6 changed** (by the author). S4: libm is the default in the audio path; spux math is opt-in
  (`tanhF` renamed `fastTanhF` to say so). Reasons: libm's last-bit differences between platforms are no more audible
  than spux's against libm (on the Acidus training notes, swapping the ladder's tanh moves harmonics by at most
  0.0036 dB against fit residuals of 2.5 to 2.9 dB), and libm costs a plugin nothing in size (a shared library; the
  Acidus binary is the same size either way). S6: a -110 dBFS peak threshold demanded identical bits on Acidus, where
  one ulp moves the ladder by -70 to -90 dBFS; the gate is now the sound measures added to the null test (power-
  weighted spectrum difference, 10 ms envelope difference) with tolerances 0.08 / 0.12 dB, set by measurement: libm
  rounding changes reach 0.038 / 0.066 dB, the smallest planted mistakes (+0.1 dB gain, a one-sample VCA delay) 0.16 /
  0.24 dB, and a cutoff 0.5 % off, a decay 1 % off, feedback 1 % off or a 1-cent detune far more. The peak difference
  stays in the report. S5 allows the same tolerances for other plugins where rounding alone exceeds -90 dBFS.
  Acidus went back to libm everywhere (bit-identical to its original output) and keeps the caching of per-sample
  coefficient work and spux's DenormalGuard.
- 2026-10-20: migration step 5 (`spux_params`, `spux_state`, `spux_clap`; Tearwash as pilot). No decision changed. New
  choices within the guidelines: the `.def` row is Plumeflow's ten columns plus `module` and a smoothing policy type
  (S10 per parameter); the flags keep Plumeflow's names (`PF_AUTO` ...) so its table moves unchanged; `PF_HOST_PLAIN`
  (an addition) keeps a parameter's host value plain where Plumeflow's rule would normalize it (Tearwash's gains,
  automated in dB); the store gets a version counter for change-driven redraws; extensions are offered by concept
  detection on the plugin class (P1: nothing to register, every extension still the plugin's to replace). All three are
  header-only. Differences from the originals a plugin meets when it switches: an Enum's label index counts from `min`
  (Plumeflow's enums all start at 0), `parseValue` refuses non-finite numbers, and `sanitizePlain` sends ±infinity to the
  default like Plumeflow's `sanitize` (Tearwash keeps clamping infinities to the range end in its own `twClamp`, so "-inf
  dB" stays the floor). Tearwash's state magic is `TWR1`, version 1 (by the author: Tearwash was unreleased, so the container starts the
  numbering; its text state from before the container still loads, recognized by the missing magic).
- 2026-10-20: **P4 changed** (by the author): user presets are human-readable text files keyed by parameter key, not
  the state container: they can be read, diffed, edited and mailed, and keys survive any table change that only
  appends. Factory presets stay compiled in, in the plugin's own compact form (Tearwash's are changes on top of a
  program, about 10 KB for 75 presets; the container would be no smaller and would lose the "missing means the
  program's setting" meaning). spux gets the shared text format, `spux/state/TextPreset.hpp` (header line with
  format and version, `key=value` lines, locale-free numbers); Tearwash's `.twpreset` uses it unchanged, byte for byte.
- 2026-10-20: migration step 6 (`spux_ui_core`, the immediate-mode acceptance gate). No decision changed; U7 stays
  conditional until the author has seen the gate (`docs/UI_GATE.md`). Choices within the guidelines: `Gui<Style>` is a
  thin template over a compiled `GuiCore`, so a Style is a plain struct of painters and a missing painter fails to
  compile only where a page uses that widget (U4); the CPU caches are three layers (background, widgets, a display layer
  used only while an overlay is up) with per-widget skip by a hash of the painter state, so cached widget areas must not
  overlap (panels and printed labels go to the background); painters may answer geometry questions (a knob's depth
  handle, a selector's cells, a popup's rows); the editor shell's root view reports a content version, an animation flag
  and a tick hook besides drawing; the telemetry ring is in `spux_core` (the audio side needs no UI target). Parameter id
  0 is valid: "no parameter" is `kNoParamId`.
- 2026-10-20: **U16 added** (by the author): the optimization of UI code follows the plugin's kind, size for a simple Retro
  plugin, speed for a large one. spux's UI targets (`spux_ui_core`, `spux_platform`, `spux_ui_classic`, `spux_ui_modern`) take
  `SPUX_UI_OPTIMIZE` (`AUTO` picks `SIZE` for `SPUX_UI_FLAVOR=RETRO`). The gate measured the trade (docs/UI_GATE.md).
- 2026-10-20: **U7 decided** (by the author, after the gate in docs/UI_GATE.md): immediate mode, accepting its size cost for
  page code that is simpler to read and change and frames that are faster than the retained panels'. **U9 extended**
  (by the author): cascading submenus become part of spux's popup (kept simple: one popup API, no plugin overlays).
- 2026-10-20: house style (by the author, PR review): US English everywhere; library comments document the library and
  never name a plugin or cite a decision id (CLAUDE.md rules 14 and 15). The provenance notes moved from the headers to
  docs/MIGRATION.md. `fft` moved from `spux_ui_core` and `spux_testing` (two copies) to `spux/dsp/Fft.hpp`. The UI gate
  (`tests/gate/`) is marked temporary. No decision changed.
- 2026-10-20: the UI gate code (`tests/gate/`, `tools/gate_report.sh`, `tools/gate_lines.py`) removed at the author's word,
  since the later moves do not need it; numbers and method stay in docs/UI_GATE.md, the code in history at 313587b.
- 2026-10-20: migration step 7, the Modern flavor (`spux_ui_modern`). No decision changed. Choices within the guidelines:
  the software fallback (U5) is a CPU renderer of its own on `CpuLayers`, not a software OpenGL: the fallback exists for
  machines where OpenGL is missing or broken, spux loads GL at run time and takes no dependency for one, and a CPU
  renderer also gives driver-independent output for headless golden frames and documentation screenshots. It draws
  text from the same SDF atlas as the GPU (shared glyph shapes and advances) and leaves the effects plain. `ModernStyle`
  is configured by a `ModernConfig` (data: the core `Theme` plus shapes, effects and control proportions), with the
  painters as public members so a plugin can replace or derive one (U8). The GPU post chain and backdrop constants
  became `GpuRenderer::Look` (defaults unchanged). The GPU path in the editor shell and window scaling (U11) are not
  built yet; `Editor<ModernStyle>` runs on `SoftRenderer`.
