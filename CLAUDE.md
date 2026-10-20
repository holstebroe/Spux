# Development guidelines (humans and AI agents)

spux (Signal Processing & User eXperience) is a shared C++20 library for the author's audio plugins
(CLAP, VST3 through clap-wrapper): DSP building blocks, parameters and state, CLAP glue, windowing and
an editor UI in three flavors (Retro, Classic, Modern). No third-party DSP or GUI frameworks (no JUCE).
MIT licence.

## Where things are

- `docs/DESIGN_GUIDELINES.md`: the rules. Read the section for the part you change.
- `docs/DECISIONS.md`: every decision with status. Do not reverse a Decided item without the author;
  update the row and the history when a decision changes.
- `docs/MIGRATION.md`: what moves in from which plugin, and the order.
- Plugins consume spux as the submodule `extern/spux`, pinned to a tag.

## Rules

1. **The plugin decides.** spux offers; it never forces a feature, flavor or policy. Nothing a plugin
   does not call may end up compiled into it: header-only templates, small separate targets, no static
   registries, large data in its own translation unit.
2. **Sound compatibility.** In existing code only bug fixes, optimizations, inaudible rounding changes
   and additions are allowed. A different curve, law, tuning, shape or default is a new class, policy
   or option, and the old one stays the default. Parameter laws and smoothing policies count as sound.
3. **Acidus is strict.** Its calibration must survive. Any change that can reach Acidus's output must
   pass its `reference_conformance_test` and its null test within the sound tolerances, and every
   difference is reported to the author per patch. Never accept an Acidus difference silently.
4. **Check sound changes.** Kernel references and golden hashes in spux; reference renders and the null
   test in each plugin. Update references in the same commit as the change, with a changelog line.
5. **Real time.** Nothing on the audio thread allocates, locks (except `try_lock`), throws, makes system
   calls or touches the GPU. Flush denormals.
6. **Determinism.** Bit-identical output within one build for any block size. PCG32 for randomness.
   libm is fine in the audio path; `spux_core` math is opt-in where `spux_bench` shows a gain that matters
   (`fastTanhF`) [S4]. Never override the flags from `spux_target_flags` (`-ffp-contract=off`, `/fp:precise`).
7. Derive coefficients from times and frequencies, never from a fixed sample rate.
8. **Instances never affect each other's output.** No state shared between instances that is written
   after initialization and can change what an instance computes, or that makes the audio thread wait
   or allocate. Allowed: immutable data, and caches of pure functions keyed on every input, filled off
   the audio thread, read without blocking, recomputed on a miss (guidelines 1.6). Several instances
   of one plugin in a DAW are normal. Plugins and spux versions are kept apart by hidden visibility and
   the inline version namespace on everything [L13].
9. **UI.** One flavor per build (`<PLUGIN>_UI=RETRO|CLASSIC|MODERN|NONE`). Immediate-mode `Gui`;
   widgets draw through per-widget painters, a flavor's look is a `Theme` data struct, and every
   painter stays inside its bounds. Modern falls back to the Modern style on the software renderer.
   GPU code never decides what the sound does.
10. **Parameters and state.** Never renumber parameter ids or rename keys; only append. Never break
    loading of older state versions.
11. Character code (a plugin's own filters, engines, reverbs) stays in the plugin. Extract on the third
    copy, or when two copies are identical.
12. Commercial-only code belongs in the private `spux-commercial`, never here.
13. Commits small and focused; keep `docs/` in step with the code in the same commit.

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

After DSP changes run the DSP tests and `spux_bench` (Release). After UI changes run the GUI tests
(golden frames, software and offscreen GPU).

## For plugin repositories

Add this section to each plugin's CLAUDE.md:

> **Using spux.** Shared code lives in spux (`extern/spux`, pinned to a tag). Change shared code in spux
> (develop with `-DSPUX_SOURCE_DIR=...`), release a tag, then bump the submodule here and run this
> plugin's null test against its reference renders. Accept differences by regenerating the references
> in the same commit as the pointer bump. This plugin's UI flavor: `<flavor>`. Character code stays in
> this repository.

For Acidus add: "Strict gate: every spux bump runs `reference_conformance_test` and the null test within
its sound tolerances; report every difference per patch to the author before accepting."
