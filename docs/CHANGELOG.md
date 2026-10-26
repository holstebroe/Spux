# spux changelog

Sound-relevant changes: every change of a kernel reference (tests/kernels/) gets a line here, in the same
commit as the regenerated files (guidelines section 3). Newest first.

## Unreleased

- 2026-10-26: a knob dragged with Ctrl held snaps to the nearest of `KnobOptions::snap` (plain values; empty: minimum,
  default and maximum). `WindowEvents::onCtrl` reports the Ctrl key before each mouse press and move (Win32, X11).
  Additions: a drag without Ctrl is unchanged.

- 2026-10-25: the Modern tooltip rounds its width up, so its title no longer loses its last letters to an ellipsis.

- 2026-10-25: files dropped on the editor window (`EditorOptions::acceptFiles`, `WindowOptions::acceptFiles`,
  `RootViewBase::filesDropped`; Win32, X11 is a TODO), the clipboard (`spux/platform/Clipboard.hpp`: a bitmap such as a
  screen capture, or copied files; Windows, nothing elsewhere), and file dialogs with several extensions (`"png;jpg"`).
  Additions: nothing that was delivered before changes.

- 2026-10-25: a Win32 window that takes the keyboard answers `WM_GETDLGCODE` with `DLGC_WANTALLKEYS`, so hosts that show plugins
  in a dialog pass shortcuts such as ctrl+V on to it.

- 2026-10-25: Modern arrow buttons (`kButtonArrowLeft`, `kButtonArrowRight`, the preset bar's previous and next) point the
  way they are named; they were mirrored. A UI bug fix: no sound changes.

- 2026-10-25: the editor window reports ctrl+letter shortcuts as a `Char` key event with `ctrl` set (X11 and Win32); text entry
  ignores them. An addition: nothing that was delivered before changes.

- 2026-10-25: `spux_presets` (header-only) adds `PresetLibrary`: factory presets from a folder of text files, a baked table when
  there is none, and in a development build (`SPUX_PRESETS_DEV`) saving a preset back to its file and adding one; with
  `tools/bake_presets.py` and `cmake/SpuxPresets.cmake`. An addition: no sound changes.

- 2026-10-22: Classic gets a third panel surface, `PanelSurface::Brushed` (`BrushedSteel`, `paintBrushedPanel`,
  `ShadedRenderer::brushedPanel`), an addition: the slate gradient with a fine grain along x, soft raked-light patches and
  a few specks, cached per size like the worn panel. Existing looks and their golden hashes are unchanged.

- 2026-10-20, migration step 5: `spux_params` brings parameter laws and host mapping (count as sound, S1):
  Plumeflow's Linear, Log, LogZero and Db, bit-identical to its `plugin/Params` over its whole table. No kernel
  reference changed.
- 2026-10-20, migration step 4: `spux_core` (Math, Pcg32, ControlTick, Smoothing, DenormalGuard) and
  `spux_dsp` (OnePoleTpt, Svf, Biquad, DcBlock, DelayLine, HalfbandStage and Oversampler, Blep 2/4/16/32,
  pt2 MinBlep, saturators with Adaa1 and the Newton and fixed-point solvers, white and pink noise, OU
  drift). First kernel references recorded in tests/kernels/ (portable.txt; GCC 13 and Clang 18 on x64
  agree). Every merged function is bit-identical to its Lumeoid, Plumeflow or Paulascape original
  except the OU setup (expD instead of std::exp, 1e-11 cents); see DECISIONS history 2026-10-20 for the
  differences a plugin meets when it switches.
