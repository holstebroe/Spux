# What moves into spux, from where

Findings from the analysis of the plugin repositories (October 2026). "Copies" are files that differ
only in namespace and include guards, or not at all.

## Inventory

| spux part | Source today | Notes |
| --- | --- | --- |
| Classic drawing (`Graphics`, `Font`, `FileDialog`, `modern/ModernDraw`, `DisplayFont`, `LabelFont`) | Acidus, Tannhauser, Tearwash | Three copies; Tannhauser and Tearwash byte-identical. Rename `modern/` to `classic/`. |
| Classic CPU renderer | Plumeflow `ui/SoftRenderer` + the files above | Merge into one supersampled renderer with shading helpers. |
| Retro style and fonts | Acidus `RetroSkin`, Syrebas and Gritbaal renderers, Paulascape `Framebuffer` and pt2 font, Kikset `Graphics` | Four drifting forks of the same `Graphics`. |
| Skin interfaces | Acidus `IGuiSkin`, Gritbaal `IControlRenderer`, Syrebas `IControlRenderer` | Become the per-widget painter interfaces. |
| Native window | Plumeflow `gfx/NativeWindow*` (ported from Acidus); `GuiWindow` code in Acidus, Gritbaal, Syrebas, Tannhauser, Tearwash, Kikset, Paulascape | Plumeflow's version is the base; Cocoa missing. |
| GL context and function table | Plumeflow `gfx/GlApi`, `GlContext_{wgl,glx,none}` | Generic as is. |
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

1. Create the repository: README, these documents, CMake skeleton, `spux_target_flags`, CI.
2. Classic drawing code (byte-identical copies): no sound risk. Pilot: Tearwash drops its vendored GUI.
3. Platform: Plumeflow `NativeWindow`; move the other `GuiWindow`s onto it.
4. `spux_core` and `spux_dsp`: merge the duplicates, add kernel references, replace libm in the older
   plugins. Acidus runs its strict gate.
5. `spux_params`, `spux_state`, `spux_clap`.
6. `spux_ui_core` from Plumeflow's `Gui`; run the immediate-mode acceptance gate on one Tannhauser and
   one Acidus panel.
7. Flavors: Retro and Classic painters from the existing skins; Modern from Plumeflow.
8. Plumeflow and Lumeoid move onto spux (Lumeoid's GPU UI starts on spux directly).

lexwash starts on spux from its first line.
