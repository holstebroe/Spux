# What moves into spux, from where

Findings from the analysis of the plugin repositories (October 2026). "Copies" are files that differ
only in namespace and include guards, or not at all.

## Inventory

| spux part | Source today | Notes |
| --- | --- | --- |
| Classic drawing (`Graphics`, `Font`, `FileDialog`, `modern/ModernDraw`, `DisplayFont`, `LabelFont`) | Acidus, Tannhauser, Tearwash | Three copies; Tannhauser and Tearwash byte-identical. Renamed `modern/` to `classic/`. **Moved to `spux_ui_classic` (step 2); Tearwash migrated.** |
| Classic CPU renderer | Plumeflow `ui/SoftRenderer` + the files above | Merge into one supersampled renderer with shading helpers. |
| Retro style and fonts | Acidus `RetroSkin`, Syrebas and Gritbaal renderers, Paulascape `Framebuffer` and pt2 font, Kikset `Graphics` | Four drifting forks of the same `Graphics`. |
| Skin interfaces | Acidus `IGuiSkin`, Gritbaal `IControlRenderer`, Syrebas `IControlRenderer` | Become the per-widget painter interfaces. |
| Native window | Plumeflow `gfx/NativeWindow*` (ported from Acidus); `GuiWindow` code in Acidus, Gritbaal, Syrebas, Tannhauser, Tearwash, Kikset, Paulascape | Plumeflow's version is the base; Cocoa missing. **Moved to `spux_platform` (step 3); Tearwash migrated.** |
| GL context and function table | Plumeflow `gfx/GlApi`, `GlContext_{wgl,glx,none}` | Generic as is. **Moved to `spux_ui_modern` (step 3), namespace `spux::gl`.** |
| Renderer interface | Plumeflow `ui/IRenderer` | Generic as is. |
| GPU renderer, shaders, SDF fonts | Plumeflow `ui/GpuRenderer`, `ui/shaders`, `ui/FontAtlas`, `ui/InterFont`, `tools/make_fonts.py`, `cmake/EmbedShaders.cmake` | Generic as is. |
| Immediate-mode `Gui` | Plumeflow `ui/Gui` | Needs: painter split, parameter binding through spux, plugin-specific ids out (10 `plugin::P_*` uses), modulation as an optional interface. |
| Editor shell | Plumeflow `ui/Editor` | Logical size and root view become parameters. |
| Editor host | Plumeflow `ui/EditorHost` | Split into a core and optional extensions. |
| Edit queue, telemetry ring | Plumeflow `plugin/EditQueue`, `plugin/Telemetry` | Ring becomes a template on the frame type. |
| Generic live views | Plumeflow `ui/Live`, `ui/Views` (spectrum, traces, meters, curve editor) | Attractor, envelope and filter views stay in Plumeflow. |
| Parameters | `ParamInfo` in Tannhauser, Plumeflow, Lumeoid, Kikset, Gritbaal; X-macro in Plumeflow and Lumeoid | Plumeflow `params.def` + `ParamStore` as the base. |
| State container | Lumeoid `LMN1`, Plumeflow `PLF1`, Rodee `StateChunk` | Lumeoid as the base. |
| PCG32 | Lumeoid, Plumeflow | Merge. |
| Math approximations | Lumeoid `dsp/math`, Plumeflow `dsp/Math`, Tannhauser `tanhFast` | Merge; replace `std::tanh` in Acidus, Gritbaal, Syrebas, Kikset. |
| Half-band oversampler | Plumeflow (4+4 all-pass sections), Lumeoid (5+5) | Merge into one design. |
| BLEP | Plumeflow (orders 2/4/16/32), Tannhauser `blep4`, Paulascape (pt2-clone, BSD-3) | Plumeflow as base; pt2 table in its own licensed folder. |
| One-pole, biquad, SVF, delay line | Acidus, Gritbaal, Tannhauser `Dsp.hpp`, Lumeoid, Plumeflow | Generic forms only; character filters stay in plugins. |
| Ramps and smoothing | Lumeoid `ramp`, Tannhauser `onePoleCoeff` | Become smoothing policies. |
| Denormal guard | Plumeflow, plus inline code in Acidus, Tannhauser, Tearwash | One class. |
| Allocation guard | Lumeoid, Plumeflow | `spux_testing`. |
| WAV, loudness, metrics | Kikset `WavWriter`, Tannhauser `Loudness`, Tearwash `analysis/Metrics` | `spux_testing`. |
| MIDI files | Acidus `MidiExport`, Paulascape `midi_export`, `midi_file_reader` | `spux_midi`. |
| CLAP smoke host | Kikset `clap_smoke_main`, Paulascape `clap_host_test_main`, Mimicry `host/clap` | `spux_testing`. |
| Packaging | `DeployClap.cmake` in Acidus, Kikset, Paulascape, Tannhauser, Tearwash | spux CMake helpers. |
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
4. `spux_core` and `spux_dsp`: merge the duplicates, add kernel references, replace libm in the older
   plugins. Acidus runs its strict gate.
5. `spux_params`, `spux_state`, `spux_clap`.
6. `spux_ui_core` from Plumeflow's `Gui`; run the immediate-mode acceptance gate on one Tannhauser and
   one Acidus panel.
7. Flavors: Retro and Classic painters from the existing skins; Modern from Plumeflow.
8. Plumeflow and Lumeoid move onto spux (Lumeoid's GPU UI starts on spux directly).

lexwash starts on spux from its first line.
