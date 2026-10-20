# What moves into spux, from where

Findings from the analysis of the plugin repositories (October 2026). "Copies" are files that differ
only in namespace and include guards, or not at all.

## Inventory

| spux part | Source today | Notes |
| --- | --- | --- |
| Classic drawing (`Graphics`, `Font`, `FileDialog`, `modern/ModernDraw`, `DisplayFont`, `LabelFont`) | Acidus, Tannhauser, Tearwash | Three copies; Tannhauser and Tearwash byte-identical. Renamed `modern/` to `classic/`. **Moved to `spux_ui_classic` (step 2); Tearwash migrated.** |
| Classic CPU renderer | Plumeflow `ui/SoftRenderer` + the files above | Merge into one supersampled renderer with shading helpers. |
| Retro style and fonts | Acidus `RetroSkin`, Syrebas and Gritbaal renderers, Paulascape `Framebuffer` and pt2 font, Kikset `Graphics` | Four drifting forks of the same `Graphics`. |
| Skin interfaces | Acidus `IGuiSkin`, Gritbaal `IControlRenderer`, Syrebas `IControlRenderer` | Become the per-widget painter interfaces. **Interfaces in `spux_ui_core` (step 6)**; the skins become flavor painters in step 7. |
| Native window | Plumeflow `gfx/NativeWindow*` (ported from Acidus); `GuiWindow` code in Acidus, Gritbaal, Syrebas, Tannhauser, Tearwash, Kikset, Paulascape | Plumeflow's version is the base; Cocoa missing. **Moved to `spux_platform` (step 3); Tearwash migrated.** |
| GL context and function table | Plumeflow `gfx/GlApi`, `GlContext_{wgl,glx,none}` | Generic as is. **Moved to `spux_ui_modern` (step 3), namespace `spux::gl`.** |
| Renderer interface | Plumeflow `ui/IRenderer` | Generic as is. **Moved to `spux_ui_core` (step 6)**, plus `layers()` for the CPU caches. |
| GPU renderer, shaders, SDF fonts | Plumeflow `ui/GpuRenderer`, `ui/shaders`, `ui/FontAtlas`, `ui/InterFont`, `tools/make_fonts.py`, `cmake/EmbedShaders.cmake` | Generic as is. |
| Immediate-mode `Gui` | Plumeflow `ui/Gui` | Needs: painter split, parameter binding through spux, plugin-specific ids out (10 `plugin::P_*` uses), modulation as an optional interface. **Done in `spux_ui_core` (step 6).** Plumeflow's drawing becomes the Modern painters (step 7); its `displayText`, sync dimming, effect presets and route table stay in Plumeflow. |
| Editor shell | Plumeflow `ui/Editor` | Logical size and root view become parameters. **Done (step 6)** for CPU renderers; the GPU path and window scaling come with Modern (step 7). |
| Editor host | Plumeflow `ui/EditorHost` | Split into a core and optional extensions. **Done (step 6)**: core, `ModulationHost`, `PresetHost`, `StoreEditorHost<N>`. |
| Edit queue, telemetry ring | Plumeflow `plugin/EditQueue`, `plugin/Telemetry` | Ring becomes a template on the frame type. **Edit queue moved to `spux_params` (step 5); Tearwash migrated.** **Telemetry ring done (step 6)**: `spux/core/TelemetryRing.hpp` (`TelemetryRing<Frame>`, `SampleRing`). |
| Generic live views | Plumeflow `ui/Live`, `ui/Views` (spectrum, traces, meters, curve editor) | Attractor, envelope and filter views stay in Plumeflow. **Consumers moved (step 6)**: `Spectrum`, `LevelMeter`, `History` in `spux/ui/Live.hpp`; the views (`ui/Views`) with the Modern style in step 7. |
| Parameters | `ParamInfo` in Tannhauser, Plumeflow, Lumeoid, Kikset, Gritbaal; X-macro in Plumeflow and Lumeoid | Plumeflow `params.def` + `ParamStore` as the base. **Moved to `spux_params` (step 5); Tearwash migrated.** |
| State container | Lumeoid `LMN1`, Plumeflow `PLF1`, Rodee `StateChunk` | Lumeoid as the base. **Moved to `spux_state` (step 5); Tearwash migrated (`TWR1`).** |
| PCG32 | Lumeoid, Plumeflow | Merge. **Merged into `spux_core` (step 4).** |
| Math approximations | Lumeoid `dsp/math`, Plumeflow `dsp/Math`, Tannhauser `tanhFast` | Merge; replace `std::tanh` in Acidus, Gritbaal, Syrebas, Kikset. **Merged into `spux_core` (step 4)**; the libm replacement happens in each plugin's step. |
| Half-band oversampler | Plumeflow (4+4 all-pass sections), Lumeoid (5+5) | Merge into one design. **Done (step 4): Lumeoid's 5+5, by measurement (docs/BENCH.md).** |
| BLEP | Plumeflow (orders 2/4/16/32), Tannhauser `blep4`, Paulascape (pt2-clone, BSD-3) | Plumeflow as base; pt2 table in its own licensed folder. **Done (step 4)**: `spux/dsp/Blep.hpp`, `spux/dsp/pt2/` (BSD-3). |
| One-pole, biquad, SVF, delay line | Acidus, Gritbaal, Tannhauser `Dsp.hpp`, Lumeoid, Plumeflow | Generic forms only; character filters stay in plugins. **Done (step 4)**, plus DC block, saturators with ADAA and the solvers (Gritbaal style), noise and OU drift. |
| Ramps and smoothing | Lumeoid `ramp`, Tannhauser `onePoleCoeff` | Become smoothing policies. **Done (step 4)**: `TickRamp`, `Smoothed<Policy>`, `onePoleCoeff`. |
| Denormal guard | Plumeflow, plus inline code in Acidus, Tannhauser, Tearwash | One class. **Done (step 4)**: Lumeoid's guard (it has one too, the more complete one). |
| Allocation guard | Lumeoid, Plumeflow | `spux_testing`. **Moved to `spux_testing_allocguard` (step 3a).** |
| WAV, loudness, metrics | Kikset `WavWriter`, Plumeflow `tools/Wav`, `tools/Loudness`, Tannhauser `Loudness`, Tearwash `analysis/Metrics` | `spux_testing`. **Moved (step 3a).** |
| MIDI files | Acidus `MidiExport`, Paulascape `midi_export`, `midi_file_reader`, Lumeoid `tools/midi_file`, Plumeflow `tools/MidiFile` | `spux_midi`. **Writer and readers moved (step 3a)**; what decides the notes stays in the plugin. |
| CLAP smoke host | Kikset `clap_smoke_main`, Paulascape `clap_host_test_main`, Mimicry `host/clap` | `spux_testing`. **Moved to `spux_testing_clap` and `spux_clap_smoke` (step 3a)**; Mimicry's `host/clap` not looked at yet. |
| Packaging | `DeployClap.cmake` in Acidus, Kikset, Paulascape, Tannhauser, Tearwash | spux CMake helpers. **Done (step 3a)**: `spux_deploy_clap`, `spux_package`, `spux_add_installer`. |
| Build flags | `LumeoidFlags.cmake`, `PlumeflowFlags.cmake`, Kikset inline | `spux_target_flags`. |

## Stays in the plugins

Synth engines and voice code, character filters (Acidus ladder, Stinchcombe network, coupling sections;
Gritbaal ladder and Sallen-Key; Tannhauser OTA SVF; Paula RC filters), the Tearwash reverb (its own
repository, consumed as today), Lumeoid's image pipeline, the Rodee and Paulascape importers,
plugin-specific views and page code.

## Order [M1]

1. Create the repository: README, these documents, CMake skeleton, `spux_target_flags`, CI. **Done**
   (2026-10-19): module targets as INTERFACE placeholders, `cmake/SpuxFlags.cmake`, `spux::v1`,
   `SPUX_SOURCE_DIR`, submodules `third_party/{clap,clap-wrapper,doctest}`, doctest target,
   `.clang-format`, CI (Linux, Windows, clang-format), size canary (`canary/`, budget in
   `canary/budget.cmake`, still report-only).
2. Classic drawing code (byte-identical copies): no sound risk. Pilot: Tearwash drops its vendored GUI.
   **Done** (2026-10-19): `spux_ui_classic` (STATIC) holds `Graphics`, `Font`, `FileDialog`, `ClassicDraw`
   (was `modern/ModernDraw`) and the `DisplayFont`/`LabelFont` data, namespace `spux::ui::classic`; golden
   hash test `ui_classic_golden_test`. Tearwash uses it (its `src/tearwash/gui` is gone, snapshots
   identical). Left: Tannhauser and Acidus still hold their copies (migrate in their own steps;
   Acidus's copy differs from the others by namespace only, checked). Not moved yet: Acidus's
   `ModernSkin`, `IGuiSkin`, `RetroSkin` (step 6/7).
3. Platform: Plumeflow `NativeWindow`; move the other `GuiWindow`s onto it. **Done in spux and Tearwash**
   (2026-10-19): `spux_platform` (STATIC; `spux::platform::NativeWindow`, `WindowEvents`, X11, Win32 and a
   none variant) and the GL parts of `spux_ui_modern` (STATIC; `spux::gl::GlApi`, `GlContext` for GLX, WGL
   and none). Plumeflow's `gfx_window_test` window cases became `platform_window_test`; new
   `gl_context_test`. Tearwash's `TwGui` runs on `NativeWindow` (its GUI test now also opens a real X11
   window under `xvfb-run`; snapshot identical). Left: Plumeflow switches to the spux copies (its own
   step 8 session; it keeps `src/gfx` until then), and Acidus, Gritbaal, Syrebas, Tannhauser, Kikset and
   Paulascape move their `GuiWindow`s in their own steps. `-DSPUX_WITH_X11=OFF` builds without X11 (CI job build-linux-minimal). Not done: Cocoa window and CGL context; the
   Win32 files are not compiled or run yet (no Windows toolchain here; CI on windows-latest is the check).
3a. Testing, MIDI and packaging utilities. **Done in spux** (2026-10-19): `spux_testing` (header-only: `Wav`, `Loudness`,
   `Metrics`, `RenderSafety`, `Hash` with `GoldenFile`, `NullTest`), `spux_testing_allocguard`, `spux_testing_clap` (`ClapHost`)
   and the tool `spux_clap_smoke`, `spux_midi` (`MidiFile`, `MidiNotes`, `MidiWriter`), and `cmake/SpuxPackaging.cmake`
   (`spux_deploy_clap`, `spux_package` through CPack, `spux_add_installer` for Inno Setup or NSIS) with
   `cmake/SpuxTesting.cmake`. Checked against the originals: the Paulascape writer and reader and the Plumeflow and Lumeoid
   readers agree on random files, Acidus's serializer byte for byte on a pattern, Plumeflow's loudness bit for bit and
   Tannhauser's within 1e-9 LU. **No plugin migrated.** Left, per plugin (their own steps): replace the copies of
   `alloc_guard`, `Wav`/`WavWriter`, `Loudness`, `Metrics`, `DeployClap.cmake`, the smoke mains and the MIDI code; Tannhauser's
   `presetLoudness` (the phrases and the engine) and Acidus's `renderPatternNotes` / Paulascape's note generation stay in the
   plugin. Differences a plugin will see when it switches: see the DECISIONS history of this date.
4. `spux_core` and `spux_dsp`: merge the duplicates, add kernel references, replace libm in the older
   plugins. Acidus runs its strict gate. **Done in spux** (2026-10-20), header-only, sample type a template
   parameter, policies with concepts. `spux_core`: `Math` (Lumeoid's double set, Plumeflow's float set, the
   [7/6] and [3/2] Pade tanh shapes), `Pcg32`, `ControlTick<N = 64>` and `TickRamp`, `Smoothed<Policy>` with
   `NoSmoothing`, `LinearRamp<N>`, `OnePole<Tau>`, `Slew<Rate>` and `onePoleCoeff`, `DenormalGuard`.
   `spux_dsp`: `OnePoleTpt`, `Svf`, `Biquad` with the RBJ designs, `DcBlock`, `DelayLine`, `HalfbandStage` and
   `Oversampler` (1/2/4/8; Lumeoid's 5 + 5 design, chosen by measurement, docs/BENCH.md), `Blep<2/4/16/32>`,
   `pt2::MinBlep` (BSD-3, own folder, `THIRD_PARTY_LICENSES.md`), saturator policies (`TanhSaturator`,
   `AsymmetricTanhSaturator`, `HardClipSaturator`, `PadeSaturator`) with `Adaa1`, `NewtonSolver` and
   `FixedPointSolver`, `WhiteNoise`, `PinkNoise`, `OuProcess`, `OuDrift`. Kernel references and golden hashes
   for every block in `tests/kernels/` (`dsp_kernel_test`); error tests against libm (`math_test`); alias
   tests for the saturators and BLEPs; `spux_bench` with the results in `docs/BENCH.md`. Checked against the
   originals: every merged function and block gives bit-identical output to its Lumeoid, Plumeflow or
   Paulascape source (3.7 million comparisons), except the OU setup (expD, 1e-11 cents). **No plugin
   migrated.** Left, per plugin (their own steps): switch Lumeoid and Plumeflow to these headers (renames
   and differences in the DECISIONS history of 2026-10-20); replace `std::tanh` in Acidus, Gritbaal, Syrebas
   and Kikset where `spux_bench` shows the spux shape is faster (it is, by 1.5x for `tanhF`); Tannhauser's
   `tanhFast`, `blep4`, `OnePoleLp`, `Biquad` and `onePoleCoeff` have spux equivalents; Acidus runs its strict
   gate on its switch. Open: `exp2F` is slower than glibc's `exp2f` (S4, docs/BENCH.md); Plumeflow's 4x
   oversampler is not a proper cascade (images at -36 dB, fixed when it moves to spux).
   **Acidus on spux DSP** (session 6, 2026-10-20): `extern/spux`, C++20, a null test (23 curated cases) and the
   conformance report compared number for number. Uses `DenormalGuard` in `process()` and `Blep<2>` for its PolyBLEP;
   coefficient work that only depends on knobs, constants or the rate is cached instead of running every sample.
   After S4 and S6 changed the same day, Acidus keeps libm everywhere (its output is bit-identical to before the
   migration) and its null test gates on the sound tolerances (0.08 dB spectrum, 0.12 dB envelope). The ladder could
   take `fastTanhF` (about 18 % of the engine; passes those tolerances) if the CPU is ever needed. No generic filter
   block matches Acidus's forms (its one-poles, notch and coupling sections stay). Report: Acidus
   `docs/SPUX_STRICT_GATE_2026-10-20.md`.
5. `spux_params`, `spux_state`, `spux_clap`. **Done in spux and Tearwash** (2026-10-20), all three header-only.
   `spux_params`: `ParamInfo` with Plumeflow's kinds, laws, flags and host mapping (plus `PF_HOST_PLAIN`), the
   X-macro helpers for a `.def` with Plumeflow's columns plus module and smoothing policy (`SPUX_PARAM_INFO`,
   `_ENUM`, `_COUNT_ONE`, `_SMOOTHER`), `ParamTable` (constexpr id and key lookup, `valid()`), `formatValue` and
   `parseValue`, `sanitizePlain`, `ParamStore<N>` (atomic host values and a version counter) and
   `BasicEditQueue` / `EditQueue` (`pushBegin`, `pushValue`, `pushEnd` with the gesture reserve, `freeSlots`).
   `spux_state`: `ByteWriter`, `ByteReader` (bounds-checked), `crc32`, `FourCC`, `ContainerWriter`,
   `readContainer` with the plugin's chunk handler, `hasMagic`, the `PARM` chunk (`paramPayload`,
   `readParamPayload`), `migrate()` with `Migration` steps, and the text preset format for user preset files
   (`TextPreset`, `parseTextPreset`, `writeTextPreset`, `formatNumber`, `parseNumber`; P4 as changed that day). `spux_clap`: `Plugin<Derived>` (lifecycle defaults,
   extensions offered by concept: params, state, audio and note ports, GUI with the platform window API and
   fixed-size defaults, timer; guarded calls; host helpers for flush, rescan, dirty state and timers),
   `SingleFactory<P>`, and helpers (`fillParamInfo`, `hostValueToText`, `asParamValue`, `pushParamValue`,
   `pushGesture`, `drainEdits`, `writeAll`, `readAll`). Tests: `params_test`, `state_test`, `clap_glue_test`.
   Checked against the originals: Plumeflow's laws, host mapping, value text and parsing are bit-identical over its
   whole table (262 parameters, 524 000 values each); the container and `PARM` chunk are byte-identical to
   Lumeoid's `LMN1` writer and read what it writes (200 random states).
   **Tearwash** (pilot): `TwParams.def`, values in a `ParamStore`, GUI and preset edits through an `EditQueue` (a
   producer mutex serializes its GUI and main threads), `TearwashClap` on `spux::clap::Plugin`, host state in the
   container `TWR1` version 1 (`PARM`, `PRST`); the text state `tearwash 1` from before it loads through a legacy
   reader (no magic). Its `.twpreset` reader and writer use spux's text preset code (files unchanged). `tearwash_state_test` loads states, a user preset, and the parameter info and value
   text recorded with the previous Tearwash (861300d), and a frozen version 1 container state. Audio output (every third
   preset with host automation) and the panel snapshot are bit-identical to before. Left: Plumeflow and Lumeoid
   switch in step 8 (Plumeflow's `.def` gets the module and smoothing columns; its `PLF1` has no CRC, so its switch
   needs a reader for the old chunks); Tannhauser, Acidus, Kikset and Gritbaal move their `ParamInfo` in their
   own steps; the telemetry ring and Rodee's `StateChunk` are not looked at yet.
6. `spux_ui_core` from Plumeflow's `Gui`; run the immediate-mode acceptance gate on one Tannhauser and
   one Acidus panel. **Done in spux** (2026-10-20); after the gate the author decided U7 (immediate mode) and U16.
   `spux_ui_core` (STATIC): `Geometry`, `IRenderer` (Plumeflow's, the effect calls with plain defaults,
   `drawTrace`/`drawMeter` no longer pure), `Layers` (`LayerCache`, `CpuLayers`: background, widgets and
   display layers, dirty rectangles, box-filter resolve), `Theme` (colours, metrics, interaction feel, font
   slots), `Painters` (state structs and one interface per widget), `EditorHost` (core, `ModulationHost`,
   `PresetHost`, `StoreEditorHost<N>`), `Gui` (`GuiCore` with Plumeflow's interaction, gestures, value
   entry, Tab focus, tooltips, popups, text field; `Gui<Style>` picks the painters), `Layout`, `Editor`
   (`EditorCore` with change-driven frames and the window, `Editor<Style>`, `RootView`), `Live`;
   `spux_core` gets `TelemetryRing`. Parameters are bound by id through the host's `ParamTable`; no plugin
   id is left in the core. Test: `ui_core_test` (a plain test style and renderer in `tests/ui/`).
   **Acceptance gate**: `tests/gate/` builds each plugin twice from its own checkout (`-DSPUX_GATE_ACIDUS_DIR`,
   `-DSPUX_GATE_TANNHAUSER_DIR`), with its retained `GuiWindow` and with a port on `spux_ui_core`, and
   `tools/gate_report.sh` measures them; numbers and method in `docs/UI_GATE.md`. No plugin repository was
   changed. Left: the flavors' painters and renderers (step 7, from Acidus's skins, Tannhauser's panel
   renderer and Plumeflow's look); Plumeflow switches in step 8; the GPU path in the editor shell, window
   scaling, cascading submenus in spux's popup (U9, for Tannhauser's preset menu) and the shared views (`ui/Views`).
7. Flavors: Retro and Classic painters from the existing skins; Modern from Plumeflow.
8. Plumeflow and Lumeoid move onto spux (Lumeoid's GPU UI starts on spux directly).

lexwash starts on spux from its first line.
