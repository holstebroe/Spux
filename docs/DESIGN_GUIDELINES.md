# spux design guidelines

These are the rules for building spux and for using it in a plugin. The reasons behind each rule are in
`DECISIONS.md` (ids in brackets). Where a rule and the code disagree, fix one of them in the same commit.

## 1. Principles

1. **The plugin decides.** spux offers building blocks; it never forces a feature, a flavor or a policy
   on a plugin. Anything a plugin does not call is not compiled into it [P6, U4].
2. **Pay only for what you use.** Size and build time matter: a Retro build is small, a `UI=NONE` build
   is fast (section 4).
3. **Sound is a contract.** Once a plugin is released, old projects must sound the same or
   inaudibly different. Character changes are new code, not edits (section 3).
4. **Building blocks, not character.** spux holds the generic parts. What gives a plugin its own sound
   (the Acidus ladder and Stinchcombe network, the Tannhauser OTA SVF, the Paula RC filters, the
   Tearwash reverb) stays in the plugin repository.
5. **Extract on the third copy**, or earlier when two copies are already identical.
6. **Instances never affect each other's output** [L16]. A DAW runs many instances of one plugin, and
   several plugins with different spux versions, in one process. Different plugins and spux versions
   never see each other's symbols: hidden visibility and the inline version namespace [L13] keep each
   binary's statics, tables and caches its own. Between the instances of one plugin, state that lives
   longer than an instance is allowed only in these forms:
   - **Immutable data**: tables, fonts, kernels, built at load or first use, never on the audio thread,
     never written again (spux's BLEP tables).
   - **Memo caches of pure functions**: the value depends only on the key, the key holds every input
     (parameters, sample rate, ...), so a hit gives exactly what a miss computes, whoever filled it.
     Filled off the audio thread; the audio thread reads without blocking (`try_lock` or lock-free) and
     computes the value itself on a miss, without allocating (Acidus's critical-gain tables).
   - **Operating-system registrations** that cannot be per instance (a Win32 window class): named per
     module, so another plugin never shares them.

   Not allowed: shared state whose content depends on which instance wrote it or when (a shared random
   generator, a shared smoother, a "current preset"), and anything that makes one instance wait for or
   allocate because of another. Prefer an explicit shared context created in the plugin's entry `init`,
   destroyed in `deinit` and handed to its instances, over function-local statics: the lifetime is
   clear, it can be reset in tests, and the plugin decides (principle 1). Per-thread storage in a
   plugin binary is not free: a large `thread_local` in a dynamically loaded library can be allocated on
   a thread's first access, so it does not belong on the audio thread.

### SOLID in spux

| Principle | How spux applies it |
| --- | --- |
| Single responsibility | Window, layout, interaction and drawing are separate parts. A DSP block does one job. |
| Open/closed | New behaviour is a new class, policy, widget or painter. Shipped behaviour is not edited in ways that change sound (section 3). |
| Liskov substitution | Every renderer honours `IRenderer`; effect calls have plain defaults, so any renderer can stand in. Every painter draws inside its widget's bounds. |
| Interface segregation | Small interfaces: one painter per widget type, an editor-host core plus optional extensions (modulation, presets). |
| Dependency inversion | DSP depends on nothing. Widgets depend on an abstract parameter binding, not on CLAP. Plugins depend on spux interfaces, spux never on plugins. |

## 2. Real-time and determinism rules

- **R1** Nothing on the audio thread allocates, locks (except `try_lock`), throws, makes system calls or
  touches the GPU. Debug and test builds check this (allocation guard, thread asserts) [P8].
- **R2** libm is fine in the audio path [S4]: different platforms' libm differ in the last bit, which nobody
  hears. Use `spux_core` math where `spux_bench` shows a gain that matters in a hot loop (`fastTanhF`), where
  constexpr is needed, or where the same bits on every platform are wanted.
- **R3** Output is bit-identical within one build for any block size and every re-render [S3]. Use
  PCG32 for all randomness. `spux_target_flags` sets `-ffp-contract=off -fno-fast-math` and
  `/fp:precise`; never override them.
- **R4** Derive coefficients from times and frequencies, never from a fixed sample rate.
- **R5** The control tick is a template parameter (default 64 samples); a plugin may use none [S9].
- **R6** Flush denormals around processing (`DenormalGuard`).
- **R7** The GUI reads audio-side state only through the parameter store, the edit queue and the
  telemetry rings [P7].

## 3. Sound compatibility

### What may change in existing code [S1]

| Allowed in place | Not allowed in place (make a new class, policy or option) |
| --- | --- |
| Bug fixes | A different curve, law, tuning, saturation shape or default |
| Optimizations | Changing what a parameter value means |
| Rounding changes nobody can hear | Removing or renaming anything a released plugin uses |
| New methods, overloads, policies | Changing a shipped smoothing policy's curve [S10] |

Parameter laws (normalized to plain value) count as sound: the host stores normalized automation [P2].

Before a plugin's 1.0 release, duplicates are merged freely [S2].

### How a change is checked [S5, S6]

1. **Kernel references in spux.** Each DSP building block has short reference outputs and a golden hash
   per platform and compiler. A changed hash is a change detector, not a failure: update the golden
   file in the same commit and note it in the changelog.
2. **Reference renders in each plugin.** A curated set of patches and note sequences, a few seconds
   each, stored as 32-bit float (or 24-bit) WAV in the plugin repo. The null test renders with the
   current code, subtracts the stored file and fails above the threshold (proposed -90 dBFS peak). One
   checkout is enough. Accepting a change means regenerating the references in the same commit. Where a
   one-ulp change already exceeds the peak threshold (a resonant filter amplifies rounding), gate on what
   is heard instead: the null test's spectrum and envelope tolerances (`NullOptions::maxSpectralDb`,
   `maxEnvelopeDb`), with the peak as a report.
3. **Acidus is strict.** Its calibration is the product. A spux update in Acidus must pass
   `reference_conformance_test` (its whole report compared with the stored one) and the null test within
   the sound tolerances (0.08 dB spectrum, 0.12 dB envelope: about 2x above a libm rounding change in its
   ladder and 2x below the smallest planted mistake, +0.1 dB gain or a one-sample delay), and the report
   lists every difference per patch. The author signs off changes; nothing is accepted silently.
   Bit-exactness is not required: a one-ulp change moves Acidus's ladder by -70 to -90 dBFS and its
   harmonics by about 0.003 dB.
4. **State version.** Every plugin state carries a version field and a migration hook [S7, P9], so a
   later converter can restore old behaviour or rescue old data.

## 4. Build rules: only what is used

1. **Header-only templates** for `spux_core` and `spux_dsp`. Nothing is compiled unless a plugin
   instantiates it.
2. **Small separate targets** for code with `.cpp` files (see the README table). A plugin lists the
   targets it links. There is no umbrella library.
3. **Section garbage collection**: `-ffunction-sections -fdata-sections` and `--gc-sections`; MSVC `/Gy`
   and `/OPT:REF,ICF`. `spux_target_flags` sets them.
4. **No static registries or self-registering objects.** They force code into every binary. Everything
   is reached through calls the plugin makes.
5. **Large data in its own translation unit** (fonts, tables), so it is linked only when referenced.
6. **Hidden visibility** and the inline version namespace on everything [L13].
7. **A size canary**: CI builds a minimal Retro test plugin and tracks its stripped size.
8. spux owns clap, clap-wrapper and doctest [L6]. A plugin's CMake adds spux and nothing else
   third-party for these.

## 5. DSP building blocks

- Templates on the sample type; `float` for audio and `double` for phase and coefficients by default
  [S8].
- Variants as **policies** (template parameters), in the style of Gritbaal's `TanhSaturator` and
  `NewtonSolver`: saturator shape, solver, smoothing, interpolation. Use C++20 concepts to state what a
  policy must provide.
- No virtual calls per sample.
- Every nonlinear block states its anti-aliasing (oversampling, ADAA) and has an alias test.
- Each block documents its law and constants in its header and has a kernel reference (section 3).
- First merges (before any 1.0): one PCG32, one set of math approximations, one half-band design, one
  BLEP family (orders 2, 4, 16, 32; the pt2-clone table stays in its BSD-3 folder). Done in step 4:
  `spux/core/`, `spux/dsp/`, the pt2 table in `spux/dsp/pt2/`.
- Kernel references live in `tests/kernels/` (`dsp_kernel_test`): the first output values as text and a
  golden hash per block, in `portable.txt` unless a toolchain needs its own file. A changed hash gets a
  line in `docs/CHANGELOG.md`.
- The double-precision math (`expD`, `logD`, `tanD`, ...) is for coefficients at control rate; it is slower
  than libm (docs/BENCH.md). Per sample, use the float set. All of it is opt-in; libm is the default [S4].

### Smoothing [S10]

`Smoothed<Policy, T>` with `NoSmoothing`, `LinearRamp` (per tick), `OnePole<Tau>` and `Slew`. The plugin
picks a policy per parameter. Plain helpers (`onePoleCoeff(tau, fs)`) stay available for code that
smooths by itself.

## 6. Parameters, state and CLAP

- **Parameters** come from the plugin's X-macro `.def` file [P2], one row
  `SPUX_PARAM(id, key, name, kind, min, max, law, default, flags, labels, module, smoothing)` (Plumeflow's
  columns plus module and smoothing policy; `spux/params/ParamTable.hpp`). Never renumber ids or rename
  keys; only append. The plugin expands the rows where it wants them (`SPUX_PARAM_INFO`, `_ENUM`,
  `_COUNT_ONE`, `_SMOOTHER`; `#define SPUX_PARAM SPUX_PARAM_INFO`, which every preprocessor splits alike) and checks its table with `static_assert(table.valid())`. spux provides the
  laws (Linear, Log, LogZero, Db), value text and parsing, id and key lookup, the atomic `ParamStore` and
  the SPSC `EditQueue` (values leave slots reserved for gesture begin and end).
- **Host values** are normalized for Float parameters with a non-linear law and plain otherwise
  (Plumeflow's rule); `PF_HOST_PLAIN` keeps a parameter's host value plain, for a plugin whose automation
  is already stored in plain units. Like the laws, the host mapping of a released parameter never changes.
- **State** uses the chunk container [P3] (`spux/state/Container.hpp`): a 4-byte magic chosen by the
  plugin, a version, chunks with id, length and CRC-32; the plugin's chunk handler decides what is known,
  unknown chunks are skipped and damaged ones counted. `migrate()` runs the plugin's steps from the version
  read [S7, P9]; a state from before the container (no magic) goes to the plugin's own legacy reader. Never
  break loading of older versions, and keep a test that loads states saved by earlier versions.
- **Presets** [P4]: factory presets compiled in, in whatever compact form suits the plugin (a preset can be
  a change on top of a program); user presets as human-readable text files keyed by parameter key, never
  by id: spux's text preset format (`spux/state/TextPreset.hpp`: a `# <format> v<version>` header, then
  `key=value` lines, numbers without the C locale). The host state is the container.
- **CLAP glue** [P1] is a CRTP base (`spux::clap::Plugin<Derived>`) that fills `clap_plugin_t`, forwards
  every call and offers an extension exactly when the plugin class has its methods (checked with
  concepts): params, state, audio and note ports, GUI and timer. Calls are guarded so no exception
  reaches the host. It stays thin: it owns no parameters or threads, and a plugin's own `getExtension`
  answers what it wants and falls back to `defaultExtension`.
- **Plugin IDs** [P12] are deferred until the brand name is chosen. Once released they never change.

## 7. User interface

### Flavors [U1 to U6]

| Flavor | Look | Renderer | Fonts | Scaling |
| --- | --- | --- | --- | --- |
| Retro | Pixel art, small footprint | Integer pixel renderer, no anti-aliasing | Bitmap (5x7, 7-px panel, pt2 8x8) | Whole steps |
| Classic | Shaded hardware panels: materials, lighting, worn paint | Supersampled CPU renderer (2x, box filter) with shading helpers | Pre-rasterized atlas | Steps up to a cap |
| Modern | Glass panels, glow, bloom, animated views | GPU (OpenGL 3.3); fallback: the same style on the software renderer | SDF atlas baked at editor open | Free, fixed aspect |

- A build has exactly one flavor, set by `<PLUGIN>_UI=RETRO|CLASSIC|MODERN|NONE`. Only that flavor's
  targets are compiled.
- The plugin declares which flavors it supports. spux forces none.
- A plugin written against the standard widgets moves **up** from Retro to Classic or Modern by changing
  the option. Moving down works for standard widgets; a plugin's own GPU views then look plain.
- Modern's fallback is the Modern style on the software renderer [U5]: same layout, effects drawn
  plain. Its users are remote and virtual sessions, broken drivers, and headless tests.

### Layers

```
spux_platform   NativeWindow (Win32, X11, later Cocoa) with its timer; the GL context is in spux_ui_modern
spux_ui_core    IRenderer, Gui (immediate mode), layout helpers, Theme, Editor shell, telemetry views
  flavor        Style (painters + Theme data) and Renderer:
                  Retro:   RetroStyle   + PixelRenderer
                  Classic: ClassicStyle + ShadedRenderer
                  Modern:  ModernStyle  + GpuRenderer (fallback: SoftRenderer)
plugin          Page functions, its own views, its EditorHost implementation
```

### Renderer contract

`IRenderer` has two parts: the basics (rectangles, rounded rectangles, lines, polylines, circles,
gradients, clipping, text) that every renderer implements, and the **effect extensions** (glow,
emission, blend mode, arcs, shadows, glass panels, strips, fill-under, pixel-size text, animated
background) with plain default implementations. Renderers override what they can; plugin code never
checks which renderer it runs on. Coordinates are logical pixels; colours are straight-alpha ARGB.

### Immediate mode [U7]

The plugin writes **page functions** that run every frame:

```
void filterPage(spux::ui::Gui& g, const Layout& l) {
    g.panel(l.filter, "Filter");
    g.knob(P_cutoff, l.cutoff);
    if (g.plainValue(P_mode) == ModeLadder) g.knob(P_drive, l.drive);
}
```

Each widget call reads its value from the parameter store, handles input inside its rectangle, asks the
flavor's painter to draw it, and on a drag writes the value back with gestures. `Gui` keeps only the
hot, active and focused widget ids and drag state. Overlays (popups, tooltips) are drawn at the end of
the frame and block input below them.

**Retained-style caches keep the CPU flavors cheap:**

1. Frames are drawn only when something changed: input, the parameter-store version counter, telemetry,
   or a running animation. 60 fps while active, about 20 fps idle, nothing while hidden.
2. The static background (panel, printed labels) is a cached layer.
3. A widget whose state is unchanged since the last frame skips drawing; the renderer reports the dirty
   region, so the window blits only that.
4. Widget ids come from the parameter id, or a label inside an id scope (`pushId`) in loops; a debug
   assert catches collisions.
5. Layout helpers (rows, columns, grids) return `Rect`s. A layout table can stay as data that a page
   loops over.

How `spux_ui_core` does it (`spux/ui/Gui.hpp`, `Layers.hpp`, `Editor.hpp`):

- A CPU renderer keeps three layers (`CpuLayers`): background, widgets, and display (widgets plus the
  overlays, used only while an overlay is up). A widget that changed is redrawn over a copy of the
  background under its bounds plus its painter's margin; a widget that is no longer drawn is erased.
  Therefore cached widget areas must not overlap: panels, frames and printed labels belong to the
  background (`RootView::background`). `Gui::overlapCount()` reports violations. A GPU renderer has no
  layers and draws everything every frame.
- The change test is a hash of the painter's state struct. A plugin view that is not a standard widget
  uses `Gui::view(id, area, key)` with a key of what it shows, and `Gui::interact` for its input.
- `Gui<Style>` is a thin template over `GuiCore` (the interaction, compiled once): a Style is a struct
  with a `theme` and one painter member per widget it supports. A page calling a widget the Style lacks
  does not compile (U4); a widget call can also name its own painter.
- `Editor<Style>` takes the logical size and the plugin's `RootView` (background, frame, a content
  version for what changes outside the parameters, an animation flag, a tick hook).

**Acceptance gate.** Immediate mode is adopted only if it is not noticeably more expensive than retained
mode and its layout code is as simple or simpler. The test: port one panel of Tannhauser and one of
Acidus, then compare with today's retained version:

| Measure | Pass |
| --- | --- |
| Lines of layout and widget code | Same or fewer |
| CPU per frame, Classic renderer, idle and while dragging a knob | Within about 10 % with caches and change-driven frames on |
| Binary size of the Retro build | Within about 5 % |

If it fails, spux uses a retained widget tree behind the same painter interfaces; flavors and styles
are unaffected.

### Painters and themes [U8]

- One painter interface per widget type (`KnobPainter`, `SliderPainter`, `TogglePainter`,
  `SelectorPainter`, `ButtonPainter`, `DropdownPainter`, `StepperPainter`, `PanelPainter`,
  `TextFieldPainter`, `PopupPainter`, `TooltipPainter`; `spux/ui/Painters.hpp`). A painter receives the
  renderer, the bounds and a state struct (value, hot, active, focused, modulation, label). It may answer
  geometry questions the interaction needs (a knob's depth handle, a selector's cells, a popup's rows).
- A flavor implements the painters it supports. A plugin that uses a widget its flavor lacks fails to
  compile; no other plugin is affected.
- A flavor's look is a `Theme` data struct: colours, metrics (text sizes, minimum widget sizes), fonts.
  A plugin can supply its own theme values.
- Every painter draws inside the bounds it is given (plus the declared margin), so incremental redraws
  stay clean.

### Editing helpers [U9]

Value entry by typing, Tab focus, tooltips, popups and dropdowns, double-click reset, shift for fine
adjustment, mouse wheel, right-click hooks. The plugin can override text formatting per widget.

### Editor host

The plugin implements an `EditorHost` core (parameters, edit queue, telemetry, flush request, UI state)
plus only the optional extensions it uses (modulation display, preset access). Widgets never see CLAP.
Widgets name parameters by id and reach them through the host's `ParamTable`; `StoreEditorHost<N>`
implements the parameter part over a `ParamStore` and an `EditQueue`. Value text and parsing default to
spux's and can be overridden per plugin (`formatParam`, `parseParam`) or per widget (`valueText`).

### Modern specifics [U12, U13]

- OpenGL 3.3 core through a runtime-loaded function table: no GL headers or libraries at build time, no
  glad, one table and one context per editor.
- All GPU objects are created when the editor opens and freed when it closes; nothing while closed. A
  quality governor lowers bloom and detail when frame time is over budget.
- Shaders are self-contained fragment shaders, so a Metal or WebGPU backend later is a port of the
  renderer only.
- Text is an SDF atlas baked at editor open from an embedded TTF subset (Inter, OFL). Ship the font
  licence.

### Telemetry [P7]

`spux::TelemetryRing<Frame>` (`spux/core/TelemetryRing.hpp`) is a lock-free single-producer ring, with
`SampleRing` for output samples. The audio thread writes only while the editor is visible; the frame
type is defined by the plugin. spux provides generic consumers (`spux/ui/Live.hpp`): spectrum (FFT, log
bands, attack and release smoothing), source histories, level meters.

## 8. Platforms

Windows first; Linux builds and tests in CI; macOS later (Cocoa window and CGL context still missing).
OpenGL is available in practice on all three; the software fallback covers remote and virtual sessions
and broken drivers [U14].

## 9. Testing and performance

- doctest for unit tests [L14].
- `spux_testing` provides: allocation guard, golden hashes, reference-render null test, WAV I/O,
  loudness, render safety (finite output, peak below 1 for a chord), a CLAP smoke host. The header-only
  parts are `spux_testing`; the allocation guard (`spux_testing_allocguard`, it replaces the global
  `operator new`) and the smoke host (`spux_testing_clap`, `spux_clap_smoke`) are separate targets for
  test executables, never linked into a plugin.
- The null test takes its threshold as a parameter (`NullOptions`); a case without a stored reference
  fails, and references are regenerated only on request (`SPUX_UPDATE_REFERENCES=1`), in the same
  commit as the change. Golden hash files hold one hash per case for each platform and compiler
  (`SPUX_UPDATE_GOLDEN=1`).
- GUI: golden frames rendered headlessly (software renderer) and offscreen (GPU), plus simulated input
  that checks the parameter edits produced.
- CPU budget [P11]: `spux_bench` for kernels and a fixed scenario per plugin (for example 8 voices,
  48 kHz, 128-sample blocks), Release builds only. CI fails on a regression over the stored baseline
  (proposed 10 to 15 %); absolute targets live in each plugin's plan.
- CI builds each plugin's native flavor on every push and every flavor nightly [U6].

## 10. Code style [L11, proposed]

`PascalCase` types and file names, `camelCase` functions, `kConstants`, `member_` with a trailing
underscore, lower-case namespaces (`spux::dsp`), `.hpp`/`.cpp`, `#pragma once`, `.clang-format` with an
LLVM base, 4-space indent, 120 columns, checked in CI. Comments say why, not what.

## 11. Licensing and private code

- spux is MIT. Third-party code keeps its licence in its own folder and is listed in
  `THIRD_PARTY_LICENSES.md` with what a shipped binary must include.
- Commercial-only code (licence keys, copy protection) lives in the private `spux-commercial`, which
  depends on spux. Public plugins build with a stub [L9].

## 12. Changing spux from a plugin

1. Make the change in spux (a local checkout through `SPUX_SOURCE_DIR`), with tests, kernel references
   and docs.
2. Release a tag.
3. In the plugin: bump the submodule, run its null test (and for Acidus the strict gate), accept or
   reject the differences, commit pointer and references together.
