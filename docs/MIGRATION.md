# What moves into spux, from where

Findings from the analysis of the plugin repositories (October 2026). "Copies" are files that differ
only in namespace and include guards, or not at all.

## Inventory

| spux part | Source today | Notes |
| --- | --- | --- |
| Classic drawing (`Graphics`, `Font`, `FileDialog`, `modern/ModernDraw`, `DisplayFont`, `LabelFont`) | Acidus, Tannhauser, Tearwash | Three copies; Tannhauser and Tearwash byte-identical. Renamed `modern/` to `classic/`. **Moved to `spux_ui_classic` (step 2); Tearwash migrated.** |
| Classic CPU renderer | Plumeflow `ui/SoftRenderer` + the files above | Merge into one supersampled renderer with shading helpers. **Done (step 7, Classic): `ShadedRenderer` (the basics and the shading helpers over the Classic drawing code); Plumeflow's `SoftRenderer` became the Modern fallback (session 9), so only the Classic drawing code was merged into it.** |
| Classic painters | Tannhauser `gui/PanelRenderer` (knob, slider cap, rocker, lever, tone and arrow buttons, panel), Acidus `gui/modern/ModernSkin` (knurled knob, lever switch, worn panel), Tearwash `TwGui` (the look Tannhauser took) | **`ClassicStyle` with a `ClassicConfig` (step 7, Classic); Tannhauser and Acidus drawn pixel for pixel, Tearwash not compared.** |
| Retro style and fonts | Acidus `RetroSkin`, Syrebas and Gritbaal renderers, Paulascape `Framebuffer` and pt2 font, Kikset `Graphics` | Four drifting forks of the same `Graphics`. |
| Skin interfaces | Acidus `IGuiSkin`, Gritbaal `IControlRenderer`, Syrebas `IControlRenderer` | Become the per-widget painter interfaces. **Interfaces in `spux_ui_core` (step 6)**; the skins become flavor painters in step 7. |
| Native window | Plumeflow `gfx/NativeWindow*` (ported from Acidus); `GuiWindow` code in Acidus, Gritbaal, Syrebas, Tannhauser, Tearwash, Kikset, Paulascape | Plumeflow's version is the base; Cocoa missing. **Moved to `spux_platform` (step 3); Tearwash migrated.** |
| GL context and function table | Plumeflow `gfx/GlApi`, `GlContext_{wgl,glx,none}` | Generic as is. **Moved to `spux_ui_modern` (step 3), namespace `spux::gl`.** |
| Renderer interface | Plumeflow `ui/IRenderer` | Generic as is. **Moved to `spux_ui_core` (step 6)**, plus `layers()` for the CPU caches. |
| GPU renderer, shaders, SDF fonts | Plumeflow `ui/GpuRenderer`, `ui/shaders`, `ui/FontAtlas`, `ui/InterFont`, `tools/make_fonts.py`, `cmake/EmbedShaders.cmake` | Generic as is. **Moved to `spux_ui_modern` (step 7).** |
| Software fallback | Plumeflow `ui/SoftRenderer`, `ui/Font` | **Rewritten as `spux::ui::modern::SoftRenderer` (step 7)** on `CpuLayers`, with the atlas font instead of the 5x7 bitmap font. |
| Modern painters | The inline drawing in Plumeflow `ui/Gui.cpp` (knob, toggle, segmented, dropdown, stepper, slider, button, chip, panel, popup, tooltip, text field) | **`ModernStyle` with a `ModernConfig` (step 7).** |
| Immediate-mode `Gui` | Plumeflow `ui/Gui` | Needs: painter split, parameter binding through spux, plugin-specific ids out (10 `plugin::P_*` uses), modulation as an optional interface. **Done in `spux_ui_core` (step 6).** Plumeflow's drawing becomes the Modern painters (step 7); its `displayText`, sync dimming, effect presets and route table stay in Plumeflow. |
| Editor shell | Plumeflow `ui/Editor` | Logical size and root view become parameters. **Done (step 6)** for CPU renderers; **the GPU path and window scaling done in step 7** (`FrameSurface` in the core, `ModernSurface` in `spux_ui_modern`). |
| Editor host | Plumeflow `ui/EditorHost` | Split into a core and optional extensions. **Done (step 6)**: core, `ModulationHost`, `PresetHost`, `StoreEditorHost<N>`. |
| Edit queue, telemetry ring | Plumeflow `plugin/EditQueue`, `plugin/Telemetry` | Ring becomes a template on the frame type. **Edit queue moved to `spux_params` (step 5); Tearwash migrated.** **Telemetry ring done (step 6)**: `spux/core/TelemetryRing.hpp` (`TelemetryRing<Frame>`, `SampleRing`). |
| Generic live views | Plumeflow `ui/Live`, `ui/Views` (spectrum, traces, meters, curve editor) | Attractor, envelope and filter views stay in Plumeflow. **Consumers moved (step 6)**: `Spectrum`, `LevelMeter`, `History` in `spux/ui/Live.hpp`; **the generic views moved in step 7** (`spux/ui/modern/ModernViews.hpp`: spectrum, traces, stereo meter, curve editor). |
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
   display layers, dirty rectangles, box-filter resolve), `Theme` (colors, metrics, interaction feel, font
   slots), `Painters` (state structs and one interface per widget), `EditorHost` (core, `ModulationHost`,
   `PresetHost`, `StoreEditorHost<N>`), `Gui` (`GuiCore` with Plumeflow's interaction, gestures, value
   entry, Tab focus, tooltips, popups, text field; `Gui<Style>` picks the painters), `Layout`, `Editor`
   (`EditorCore` with change-driven frames and the window, `Editor<Style>`, `RootView`), `Live`;
   `spux_core` gets `TelemetryRing`. Parameters are bound by id through the host's `ParamTable`; no plugin
   id is left in the core. Test: `ui_core_test` (a plain test style and renderer in `tests/ui/`).
   **Acceptance gate**: `tests/gate/` builds each plugin twice from its own checkout (`-DSPUX_GATE_ACIDUS_DIR`,
   `-DSPUX_GATE_TANNHAUSER_DIR`), with its retained `GuiWindow` and with a port on `spux_ui_core`, and
   `tools/gate_report.sh` measures them; numbers and method in `docs/UI_GATE.md`. No plugin repository was
   changed. The gate code was removed after the decision (kept in history at 313587b; see `docs/UI_GATE.md`). Left: the flavors' painters and renderers (step 7, from Acidus's skins, Tannhauser's panel
   renderer and Plumeflow's look); Plumeflow switches in step 8; the GPU path in the editor shell, window
   scaling, cascading submenus in spux's popup (U9, for Tannhauser's preset menu) and the shared views (`ui/Views`).
7. Flavors: Retro and Classic painters from the existing skins; Modern from Plumeflow. **Classic and Modern done in spux; Retro left.**
   **Modern done in spux** (session 9, 2026-10-20), `spux_ui_modern` (STATIC, one object per part):
   `GpuRenderer` (with `Look` for the post chain), `FontAtlas` and the Inter data (own object), the shaders
   embedded by `cmake/SpuxEmbedShaders.cmake`, `tools/make_fonts.py`, `SoftRenderer` (supersample 1 to 4, shareable atlas),
   `ModernStyle` with `ModernConfig` (`standard()`, `highContrast()`), and `drawSpectrum`, `drawTraces`,
   `drawStereoMeter`, `drawCurveEditor`. Tests: `ui_modern_test` (software renderer primitives, every painter
   inside its bounds and margin, popup and tooltip layout, interaction geometry, the config, a golden frame,
   the editor shell with caches against an uncached frame) and `gpu_renderer_test` (the ported shape, text, bloom
   and scale cases, painters on the GPU, a golden frame, GPU against software). Golden frames are of a showcase
   page made for spux (`tests/golden/`, quarter size, structural similarity), not of Plumeflow's pages. Also added: `third_party/inter`,
   `third_party/stb` (stb_truetype; spux owns it now), `History::copyTo`. **No plugin changed.** Size (stripped, GCC -O2 on x64 Linux,
   with the standard library: a probe that draws eight widgets): the core `Gui` with a plain test style 109 KB; `ModernStyle` on
   `SoftRenderer` 244 KB (+135 KB: atlas baking 28 KB, the font data 34 KB, painters 50 KB, the renderer 31 KB); with the GPU
   renderer as well 310 KB (+200 KB over the core; the GPU renderer, GL context and shaders are about 65 KB). A plugin that does not link the
   flavor pays nothing: every part is its own object in a static library.
   **The GPU path in the editor shell** (same session): `FrameSurface` (`spux/ui/FrameSurface.hpp`, no GL in the core)
   is how a frame reaches the screen without being pixels; `EditorCore` takes an optional `SurfaceFactory`, opens a GL
   window, attaches the surface and, if that fails, closes it and falls back to the CPU renderer. With a surface the window
   is the logical size times the user scale (`EditorOptions::scale`, `minScale` 0.75, `maxScale` 2; fixed aspect) times the
   system scale; `canResize`, `adjustSize`, `setSize`, `setScale`, `setUserScale`, `scale`, `usesGpu`, `rendererName`,
   `degradation`; window pixels become logical pixels in the input path. `ModernSurface` (GL context, `GpuRenderer`,
   `QualityGovernor` with Plumeflow's numbers) and `modernBackend()` (the surface factory plus the `SoftRenderer`
   factory) are what a plugin passes to `Editor<ModernStyle>`. Auto does not use a software OpenGL (the CPU renderer
   is faster); `RendererChoice::Gpu` does; `SPUX_RENDERER=gpu|soft` overrides for development. Tests:
   `editor_surface_test` (a fake surface on a real X window: attach, fallback, frame order, scale and limits, pixels to
   logical pixels through synthetic X events, retry after a refused frame) and, in `gpu_renderer_test`, an `Editor<ModernStyle>` on a real
   GL window read back from the X server, at 1x and 1.25x, and the default choice (the software fallback on Mesa). Found
   on the way: right after a window of the process closed, `XOpenDisplay` was refused in about a third of the tries
   under Xvfb, which broke the fallback (GL window closed, plain window refused); the X11 window now retries briefly
   (only when a display is configured). Also fixed: clang builds (an unused field in `GuiCore`, unused GLX constants).
   Left: Retro and Classic painters; cascading submenus in the popup; Cocoa; the Win32 build of this code is unverified
   (no Windows toolchain here; CI is the check); Plumeflow's switch (step 8).
   **Classic done in spux** (session 10, 2026-10-20), in `spux_ui_classic` (STATIC, one object per part): `ShadedRenderer`
   (an `IRenderer` on `CpuLayers`, supersample 1 to 4, default 2 with the box filter; analytic edges for the basics, clip for
   everything including text and shading; the shading helpers: `graphics()`, `ink`, `inkCentered`, `wornPanel` with a per-renderer
   cache, the two atlas fonts), `ClassicStyle` (`ClassicKnob`, `ClassicSlider`, `ClassicToggle`, `ClassicSelector`,
   `ClassicButton`, `ClassicPanel`, and `drawLed`) with `ClassicConfig` (`slate()`, `silver()`), and `classicRenderer()` for
   `Editor<ClassicStyle>`. `ClassicDraw` gained `shadeRoundRect` and `softShadow` (the third copies: Tannhauser, Tearwash and
   Acidus each had them) and `Graphics` a clip (`setClip`; the default is the whole buffer, so the golden hash of the drawing
   code is unchanged). Tests: `ui_classic_style_test` (renderer primitives and clipping, the worn-panel cache, every painter
   inside its bounds plus margin in both looks and at four sizes, no drawing on another renderer, the lamp, the geometry the
   interaction asks for, a knob drag, selector and toggle through the `Gui`, `configure()`, the showcase as golden frames in
   both looks, the plugin canvases as golden frames, the editor shell with caches against an uncached frame). The golden
   frames are quarter-size pixmaps in `tests/golden/` (structural similarity). **No plugin changed.** Against the plugins'
   own code (a harness outside the repository compiles Tannhauser's `PanelRenderer.cpp` and Acidus's `ModernSkin.cpp`
   unchanged, draws the same widgets on the same canvas with the painters, and diffs the frames): Tannhauser's knobs (scale and
   hover), faders (dark and colored caps, ticks, slot), rockers (well, lamp), lever, tone buttons (lit, dim, hover), arrow
   buttons (hover, pressed) and the surface are **identical to the last pixel** (largest difference 0 of 255); Acidus's worn
   panel, knurled knobs (metal and four cap colors, labels) and lever switches are identical except for what Acidus draws in its
   switch (the printed waveform symbols, which stay in the plugin). Tearwash was not compared (see the notes below). Size (stripped,
   GCC -O2 on x64 Linux, with the standard library, the same kind of probe as for Modern: eight widgets drawn once): `ClassicStyle` on
   `ShadedRenderer` 210 KB, against 109 KB for the core `Gui` with a plain test style and 244 KB for `ModernStyle` on `SoftRenderer`; the two
   font tables are in `ShadedRenderer`'s object, so a program that draws on it carries both.
   Left: the stepper, dropdown, text field, popup and tooltip painters (Tannhauser's LCD and preset readout, the Tearwash
   preset menu); a modulation display on the Classic knob; Classic scaling in steps (U11); Retro.
8. Plumeflow and Lumeoid move onto spux (Lumeoid's GPU UI starts on spux directly).

lexwash starts on spux from its first line.

## Provenance and switch notes

Library comments describe the library only; where code came from, and what a plugin meets when it switches
from its own copy, is kept here.

- **Math** (`core/Math.hpp`, decision S4): Lumeoid `dsp/math.hpp` (the double `...D` set, the [7/6] Pade tanh;
  its header claimed double precision, its test checked 1e-9), Plumeflow `dsp/Math.hpp` (the float set fitted
  by its `tools/fit_math.py`), Tannhauser's [3/2] Pade tanh. Renames: Plumeflow `exp2`, `tanh` are `exp2F`,
  `fastTanhF`; Lumeoid `tanhApprox` is `tanhPade76`; Tannhauser `tanhFast` is `tanhPade32`. `fastTanhF` was
  measured in Acidus's ladder (about 150 calls per sample, 18 % of the engine).
- **Pcg32**: Lumeoid `core/pcg32.hpp` and Plumeflow `dsp/Pcg32.hpp`, same sequence. Lumeoid `reseed(seed,
  stream)` is `seed(seed, stream)`. Plumeflow: the default constructor gives the PCG reference state, not
  seed(0, 1), so a generator used before seeding starts elsewhere; everything else is unchanged.
- **ControlTick, Ramp**: Lumeoid `Ramp`, `DoubleRamp` (bit-identical at N = 64); 64 is the tick of Lumeoid and
  Plumeflow. **Smoothing** (S10): `onePoleCoeff` from Tannhauser (expD instead of std::exp); the linear ramp
  is bit-identical to Lumeoid's on tick boundaries.
- **DenormalGuard**: Lumeoid `dsp/simd.hpp`; Plumeflow's guard only set FTZ and DAZ (same result under hosts
  that keep the default rounding and masks).
- **TelemetryRing** (P7): Plumeflow `plugin/Telemetry`.
- **DelayLine**: Lumeoid `fx::DelayLine` (its `read` is `readHermite`) and Plumeflow `DelayLine` (`push` is
  `write`; it interpolated from the older sample, so fractional reads differ in the last bit).
- **Halfband / Oversampler**: Lumeoid `dsp/oversampler.hpp` (design and structure) and Plumeflow
  `dsp/Halfband.hpp` (4 + 4 sections, -95.2 dB). 5 + 5 was chosen so images clear the -110 dBFS strict gate
  (DECISIONS history 2026-10-20). Lumeoid `Oversampler::prepare` is `setFactor`; float output is bit-identical
  to Lumeoid's.
- **OnePole**: same arithmetic as Lumeoid `fx::OnePole`, Tannhauser `OnePoleLp` and Plumeflow's tone filters.
  **Svf**: Plumeflow `NoiseFilter`; Tannhauser's saturating SVF stays in Tannhauser. **DcBlock**: Plumeflow's
  filter and drive blockers. **Biquad**: Tannhauser's `Biquad` is the same structure.
- **Blep**: Plumeflow `dsp/Blep.hpp` (its spec 04; its osc_alias_test measured the choice); `Blep<4>` is
  Tannhauser `blep4`; the table is built with spux math (Plumeflow used std::sin; last-bit difference).
  **pt2 MinBlep**: by way of Paulascape `core/blep`; Paulascape's public `samplesLeft` and `lastValue` are
  accessors.
- **Saturator**: in the style of Gritbaal's `TanhSaturator` and `NewtonSolver` (Gritbaal used std::tanh; about
  1e-7 apart). `TanhSaturator`'s antiderivative is Plumeflow `logCosh`, `AsymmetricTanhSaturator` is
  Gritbaal's diode-clipper asymmetry, `PadeSaturator` is Tannhauser `tanhFast`, `Adaa1` is Plumeflow
  `DriveStage::adaa`.
- **Noise**: Plumeflow `NoiseSource`. **Drift**: Plumeflow `OuProcess` and `Drift` (setup() uses expD, Plumeflow
  std::exp; Plumeflow maps its knob to 0..25 cents).
- **Parameters** (P2): `ParamInfo`, `ParamText`, `ParamTable` from Plumeflow `plugin/Params` and `params.def`
  (the flag names are Plumeflow's, so its table moves unchanged; the log law with an exact 0 is for its times);
  `ParamStore` from `plugin/ParamStore`, `EditQueue` from `plugin/EditQueue`; `drainEdits` in `ClapHelpers`
  is Plumeflow's.
- **State** (P3, P4, P9, S7): `Container` and `Bytes` from Lumeoid's `LMN1` (`state/container`), including the
  PARM layout; Tearwash's text state has no magic and keeps its own legacy reader. `TextPreset` is Tearwash's
  preset file format.
- **MIDI**: `MidiFile` merges Paulascape (`midi_file_reader.hpp`), Lumeoid (`tools/midi_file.cpp`) and
  Plumeflow (`tools/MidiFile.hpp`); `MidiWriter` merges Acidus (`sequencer/MidiExport.cpp`) and Paulascape
  (`export/midi_export.cpp`).
- **Testing**: `Metrics` moved unchanged from Tearwash (`analysis/Metrics.hpp`); `Loudness` merges Tannhauser
  (`tests/Loudness.hpp`) and Plumeflow (`tools/Loudness.hpp`); `Wav` PCM 16 is Kikset's layout; the legacy FNV
  basis in `Hash` is the Classic drawing golden of Tearwash, Tannhauser and Acidus; Acidus runs the null test at
  -110 dBFS / its sound tolerances and uses `formatNullReport` for its per-patch report.
- **Platform**: `NativeWindow` (X11, Win32) ported from Acidus `GuiWindow`; Win32 blits the whole frame as
  Acidus does; Cocoa is still open (U14).
- **Classic UI**: `ClassicDraw` is the drawing shared by Acidus, Tannhauser and Tearwash; the extruded panel is
  Acidus's; the thin 5x7 label font is the Syrebas panel font; `FileDialog` serves Acidus's calibration files
  and pattern banks.
- **UI core**: `Geometry`, `IRenderer`, `Gui`, `Editor`, `EditorHost` (its modulation extension is Plumeflow's
  matrix) and `Live` come from Plumeflow `ui/`; `EditQueue` gesture balancing follows Plumeflow's Gui.
- **Classic style** (`classic/ShadedRenderer`, `classic/ClassicStyle`, `classic/ClassicConfig`): the knob, slider cap, rocker,
  lever, tone and arrow buttons and the slate surface are Tannhauser `gui/PanelRenderer` (`drawKnob`, `drawSliderCap`,
  `drawRockerKey`, `drawLever`, `drawToneButton`, `drawArrowButton`, the slot, tick and scale code of `drawStatic`), which took
  its cap, disc and button look from Tearwash `TwGui`; the knurled knob, the lever switch and the worn panel are Acidus
  `gui/modern/ModernSkin`; `drawLed` is new, in the lamp style of Tannhauser's rocker lamp and memory dot. What a plugin meets when it
  switches:
  - Tannhauser: a control becomes a standard widget whose bounds the painter draws in, with what the static layer printed: the
    scale of a knob (bounds = the control's rectangle grown by 7 for a 44 px knob: the dial's radius plus 10 on every side), the
    slot and ticks of a fader (bounds = the control's rectangle, the cap 21 x 11, the slot 5 wide), the well of a rocker (bounds
    = the key's rectangle grown by 4), the slot of a lever. A cap's color is the widget's accent (`KnobOptions::accent` and
    the others): the default accent is neutral metal, so a fader or lever cap with no color of its own is the dark one (Tannhauser's
    `kCapBlack`). A fader's center tick is lit when the parameter's range crosses zero (Tannhauser asked its own `PF_BIPOLAR` flag).
    A button's pressed and hovered states are separate, as there; a knob's or fader's pressed state brightens like hover. The tone
    button's two legend lines are one label with a newline, set at 0.31 and 0.62 of the height (the same pixels at Tannhauser's
    32). The LCD, paddles, ribbon, keyboard, readout, header, walnut cheeks, waveform symbols and the name board stay in the plugin
    (drawn with `ShadedRenderer::graphics()` and the `ClassicDraw` helpers in a `Gui::view` or the background).
  - Acidus: the knurled knob's bounds are `2r + 31` wide and `2r + 31 + 15` high for the radius `r` (the dial centered on a pixel
    center, a row of 15 above it for the label, which is the label's old place); `silver()` sets that. The panel is a titleless
    `Gui::panel` over the window (trims on) and the printed divider is the plugin's. The sprite cache of finished knobs is not
    needed: `Gui` skips a widget whose state is unchanged. Labels are clipped to the widget's bounds now (the old skin drew them
    outside), so a toggle's bounds must be as wide as its label.
  - Tearwash: its look is the slate look with `knob.ticks = false`, `slider.capW = 28`, `capH = 15` and `button.radius` 4 or 5; its
    faders also print a tick scale on both sides and a slot 4 wide, which `ClassicStyle` does not offer yet, so it was not
    switched or compared.
  - Not provided yet: painters for the dropdown, stepper, text field, popup and tooltip; a page that uses those widgets does not
    compile against `ClassicStyle` until they exist (or a plugin passes its own painter).
- **Modern UI**: `GpuRenderer`, `FontAtlas`, the shaders, `InterFont` (byte for byte, namespace only) and
  `tools/make_fonts.py` come from Plumeflow `ui/` and `tools/`; `cmake/EmbedShaders.cmake` is
  `spux_embed_shaders` (it takes the output file; the raw string delimiter is `SPX`). `SoftRenderer` replaces
  Plumeflow's (bitmap font, no effects). `ModernStyle` is the drawing of Plumeflow's `Gui` (`knob`, `toggle`,
  `segmented`, `dropdown`, `stepper`, `slider`, `button`, `chip`, `panel`, `popup`, `textField`, tooltip) and the views
  are `ui/Views` (spectrum, traces, stereo meter, curve editor). What Plumeflow meets when it switches:
  - Names: `Align::Centre` is `Center`; `Theme` (one struct with an `Accents` member) is the core `Theme` plus a
    `ModernConfig`, and the section accents are `KnobOptions::accent` and the others, or `Gui::setAccent`;
    `Theme::contrast()` is `ModernConfig::highContrast()`; `segmented` is `selector`, `chip` is a button with
    `variant = kButtonChip`, a button's `active` is `ButtonOptions::on`.
  - Looks that changed: a knob's dial geometry no longer depends on `large` (a large knob has heavier strokes and
    larger text, the same dial: the interaction asks the painter without knowing the options); the slider label
    sits in a column of `Slider::labelWidth` (72) instead of one measured from the text, and the track is inset
    by the thumb radius so the thumb stays inside the bounds; popup rows, separators and disabled items are new,
    and the highlight uses the theme's accent (the popup state carries no widget accent); the knob value text
    and tooltip sizes come from `Theme::metrics`. The GPU post chain constants are `GpuRenderer::Look`, with
    Plumeflow's values as the defaults, so GPU output is unchanged until a plugin sets them.
  - Stays in Plumeflow: `displayText` (units and the sync note go in through `valueText` and `dimmed`), the
    attractor, envelope, filter response, voices, signal flow and matrix views. The editor's GPU glue and the
    governor are spux's now (`ModernSurface`, same constants); `PLUMEFLOW_RENDERER` is `SPUX_RENDERER`; the software
    path's tick (12 and 4 Hz there) is `EditorOptions::activeHz` and `idleHz`; the editor's fps cap is the plugin's choice
    of `activeHz`; `view.scale` is `Editor::setUserScale`; `g.detail` follows `degradation()`. The generic views take arrays and levels
    instead of `Gui::live()`; the curve editor keeps its drag state in a `CurveEditorState` (Plumeflow used
    function-local statics, which two editors would have shared).
  - Tests: Plumeflow's `gpu_renderer_test` cases for shapes, text, bloom and scale are in spux's; its golden frames
    are of its own pages and stay there, to be regenerated when it switches.
