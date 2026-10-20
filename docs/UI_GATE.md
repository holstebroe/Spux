# Immediate-mode acceptance gate (migration step 6, 2026-10-20)

The gate of DESIGN_GUIDELINES section 7 (decision U7): port one Tannhauser panel and one Acidus panel to the
immediate-mode API and compare with today's retained versions. **Result: lines and CPU pass, the Retro
binary size fails.** The author accepted the size cost: U7 is decided (immediate mode), and U16 builds
spux's UI code for size in Retro plugins.

**Removed after the decision.** The gate code (`tests/gate/`, `tools/gate_report.sh`, `tools/gate_lines.py`)
named and ported two concrete plugins only to take these measurements; the author had it deleted once U7 was
decided. This page keeps the numbers and the method. The code is in the history: commit 313587b, the last one
that has it (`git show 313587b:tests/gate/acidus/GuiWindow.cpp`, or `git checkout 313587b -- tests/gate tools`
with `tests/CMakeLists.txt` of that commit to run it again). The two ports are worked examples of a panel on
`spux_ui_core` for the plugins' own moves.

| Measure | Pass | Acidus | Tannhauser | |
| --- | --- | --- | --- | --- |
| Lines of layout and widget code | same or fewer | 201 vs 301 (0.67) | 419 vs 529 (0.79) | pass |
| CPU per frame, Classic, idle | within ~10 % | -26 % | -76 % (a skipped tick) | pass |
| CPU per frame, Classic, dragging a knob | within ~10 % | -71 % | -69 % (slider), knob -60 % wall | pass |
| Binary size, Retro build | within ~5 % | +71 % (+114 KB); +33 % (+53 KB) with `--gc-sections` and spux UI at `-Os` | (no Retro build; Classic +19 %, +7.6 % tuned) | **fail** |

## What was ported

Both ports lived in spux (`tests/gate/`, now removed); no plugin repository was changed. Each is a drop-in for the
plugin's `GuiWindow` (same class and calls), so the gate links the **whole plugin** twice from the plugin's
own checkout, with the plugin's own compiler settings (`-Os` GUI and CLAP code, link-time optimization):
`retained` (the plugin's `GuiWindow.cpp`) and `immediate` (the port on `spux_ui_core`). Both draw with the
plugin's own drawing code: Acidus's skins (`IGuiSkin`, Retro and Modern = Classic) and Tannhauser's
`panel::drawControl`, behind spux painter interfaces.

- **Acidus**: the whole panel: waveform switch, eight knobs, the animated logo plate (a custom view; its
  drawing is copied from Acidus as it was), the calibration label, the calibration menu (a spux popup with
  Acidus's look) and export/import through its file dialog.
- **Tannhauser**: the whole panel by looping over its existing layout table (`PanelLayout`, unchanged):
  sliders, paddles, rockers, levers, knobs, the Hold key, tone buttons with memories, preset LCD and arrows,
  ribbon, keyboard, the header readout. **Not ported**: the preset menu (two levels, cascading; spux's popup
  has one level) and Save As. Their retained code is left out of the counts.

Pixels: every step of a scripted session (`--pixels`) hashes the frame.
- Acidus, Retro and Classic: bit-identical to the retained version at every step, and the cached frames are
  identical to a full uncached redraw.
- Tannhauser: 14 of 16 steps bit-identical. The two others differ only in the 1 to 3 px shadow rings of the
  tone buttons, which Tannhauser's painter draws outside the 1 px margin its retained window declares (the
  buttons are 2 px apart), so every redraw blends the ring once more; the ring depends on how often a
  button was redrawn, in both versions. A pre-existing painter issue, not one of the API.

Reproduce (from commit 313587b): configure with `-DSPUX_GATE_ACIDUS_DIR=... -DSPUX_GATE_TANNHAUSER_DIR=...`, build `spux_gate`
(Release), run `tools/gate_report.sh <build> <Acidus> <Tannhauser>`. Linux x64, GCC 13, 4 cores, shared
machine: the wall clock is noisy, the instruction counts (callgrind) are exact.

## Lines

Non-blank, non-comment lines after formatting both sides with spux's `.clang-format`
(`tools/gate_lines.py`, which lists exactly which functions count).

| | Retained | Immediate | of which: binding, painters, page |
| --- | --- | --- | --- |
| Acidus | 301 | 201 | 53, 68, 80 |
| Tannhauser | 529 | 419 | 47, 69, 303 |

Counted on the retained side: control setup, state polling, the caches and compositing, hit tests, input
handlers, the menu's layout and hit test (Acidus). Left out on both sides: drawing (skins, panel renderer,
logo plate, menu look), window plumbing, file dialogs, the CLAP extension. Tannhauser's control table
(`PanelLayout`, 255 lines) serves both versions unchanged. The **binding** (an `EditorHost` over the
plugin's own parameter calls, 47 to 53 lines) disappears when a plugin moves to `spux_params`
(`StoreEditorHost`); without it the ratios are 0.49 and 0.70.

## CPU per frame (Classic renderer, caches and change-driven frames on)

Instructions per frame (callgrind, exact), immediate against retained:

| Scenario | Acidus Classic | Acidus Retro | Tannhauser Classic |
| --- | --- | --- | --- |
| Idle (Acidus: logo animates; Tannhauser: nothing changed) | 4.04 M vs 5.47 M (-26 %) | 4.00 M vs 5.46 M (-27 %) | 8.9 k vs 36 k (-76 %) |
| Dragging (Acidus: cutoff knob; Tannhauser: a slider) | 9.98 M vs 34.0 M (-71 %) | 6.77 M vs 49.2 M (-86 %) | 3.63 M vs 11.7 M (-69 %) |
| Hover moves between controls | | | 8.7 M vs 38.6 M (-78 %) |
| Automation (7 or 10 values change per frame) | 56.1 M vs 81.4 M (-31 %) | 27.0 M vs 60.3 M (-55 %) | 6.16 M vs 13.3 M (-54 %) |

Wall clock, median per frame (best of 5 runs of 120 to 240 frames):

| Scenario | Acidus Classic | Acidus Retro | Tannhauser Classic |
| --- | --- | --- | --- |
| Idle | 0.351 vs 0.407 ms | 0.351 vs 0.387 ms | 0.0007 vs 0.0042 ms |
| Knob drag | 1.42 vs 3.92 ms | 0.68 vs 5.06 ms | 0.92 vs 2.28 ms |
| Slider drag | | | 0.37 vs 0.78 ms |
| Hover | | | 1.02 vs 2.73 ms |
| Automation | 7.22 vs 9.53 ms | 2.26 vs 4.75 ms | 0.78 vs 1.23 ms |
| Host notes (keyboard) | | | 5.03 vs 5.44 ms |
| First frame | 100 vs 98 ms | 17.6 vs 14.5 ms | 124 vs 148 ms |

Why the port is cheaper: Acidus's retained window redraws its whole static layer and downsamples the whole
frame whenever one knob moves; the port redraws only the changed widget's area. Tannhauser's window polls
and compares every control's state on every tick and redraws inside each input handler; the port skips a
tick with nothing changed after two version checks. Two spux changes came out of the measuring: the
display layer is used only while an overlay is up (it cost a copy per frame), and the 2x box filter is
unrolled (the generic loop cost 45 % of an idle Acidus frame).

## Binary size (whole plugin, stripped)

| Build | Retained | Immediate | Change |
| --- | --- | --- | --- |
| Acidus Retro, as Acidus builds today | 160 544 | 274 752 | +114 208 (+71 %) |
| Acidus Retro, `--gc-sections`, spux UI at `-Os` | 160 544 | 213 312 | +52 768 (+33 %) |
| Acidus Classic, as today / tuned | 181 184 | 307 648 / 246 208 | +70 % / +36 % |
| Tannhauser Classic, as today / tuned | 485 600 | 575 784 / 522 536 | +19 % / +7.6 % |

"Tuned" is what a plugin on spux would do: `spux_target_flags` links with `--gc-sections` (Acidus does not
today), and the spux UI targets compiled for size as both plugins compile their own GUI. The second is now
spux's default for a Retro build (`SPUX_UI_OPTIMIZE=AUTO` with `SPUX_UI_FLAVOR=RETRO`, decision U16).

Speed of the size build (Acidus, instructions per frame, port at `-O3` / port at `-Os` / retained): Retro idle
3.99 M / 4.82 M / 5.45 M, Retro knob drag 6.8 M / 8.9 M / 49.2 M, Classic idle 4.03 M / 4.85 M / 5.47 M, Classic
knob drag 9.96 M / 11.96 M / 34.0 M. Building for size costs about 20 % of the port's UI time and stays below
the retained version in every case.

Where the Acidus Retro bytes go (tuned build, symbols): the port's own code (page, painters, binding, logo
plate) is 25 KB against the retained window's 22 KB. The rest is spux: the native window (`spux_platform`,
7 KB; the retained window had its own X11 code), the editor shell (5 KB), the layers (3 KB), the Gui
(17 KB: knob 3.8 KB, end of frame and overlays 2.2 KB, constructor 1.4 KB, toggle, interact, view, popup,
gestures, value entry and Tab focus), vtables and the renderer's default effect calls, plus 4 KB of
standard library and more unwind tables and relocations. About 15 KB of it (window, shell, layers) a
retained widget tree behind the same painters would need as well; the immediate-mode Gui is the rest.

For a plugin of Acidus's size (160 KB), any shared editor shell costs more than 5 % (8 KB). The gate as
written compares with hand-written code rather than with the fallback (a spux retained tree), which is not
built.

## The author's decisions (2026-10-20)

1. Size criterion: the measured cost is accepted for simpler, easier to change page code; U7 is decided.
2. spux's UI code for size in a simple Retro plugin, for speed in a large one (U16, `SPUX_UI_OPTIMIZE`).
3. Cascading popups (Tannhauser's preset menu) become part of spux's popup (U9).
