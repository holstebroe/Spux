# spux changelog

Sound-relevant changes: every change of a kernel reference (tests/kernels/) gets a line here, in the same
commit as the regenerated files (guidelines section 3). Newest first.

## Unreleased

- 2026-11-07: `ClassicConfig::keyPress`: how a pressed or sounding piano key looks, `KeyPress::Tint` (the accent tint, the default and the look so far), `Shade` (grey ivory, a lighter black key) or `Sunken` (felt and neighbor shadows on the key, a shorter front edge). No existing output changes.

- 2026-11-07: Win32 window: a window class left registered by an earlier load of the same DLL at the same address (Windows does not unregister a DLL's classes when it unloads) is replaced instead of failing every later editor until the host restarts.

- 2026-11-07: `ShadedRenderer` keeps the last four worn or brushed panel textures instead of one. A page that draws a window panel and a smaller untitled panel rendered the window's texture again every frame (about 75 ms for a 900 x 700 brushed panel); now each is rendered once. Same pixels.

- 2026-11-02: `ClassicConfig::buttonCap` (default 5, the size so far): label cap height of plain buttons and tab cells, so a plugin with a larger window can print larger button lettering. No existing output changes.

- 2026-11-01: lower case and the characters between Z and a in the Classic label and display fonts (ASCII 91 to 126; `tools/extend_gui_fonts.py`
  adds them to the atlas without touching the existing glyphs). `ShadedRenderer::setMixedCase(true)` draws `drawText` / `drawTextPx` lower case with
  its own glyphs; off by default, so lower case still draws as capitals and no existing output changes. `classic::textWidth` and `drawText` take
  an optional `mixedCase` argument.

- 2026-11-01: popup lists taller than the view scroll (all flavors). `GuiCore` shows a window of the rows: the wheel scrolls the
  list under the pointer, Up/Down/Home/End move through the list that has the keyboard, a row at each end scrolls while the pointer
  rests on it, a thin marker shows the position, and the current choice is in view on open. The painters get the sliced rows, so
  `choose`, `choosePath` and submenu routes still report indices of the whole list. `PopupPainter::paintScrollArrow` and
  `paintScrollMarker` are optional hooks with defaults; Classic, Modern and Retro draw their own marker (Retro whole pixels).
  `GuiCore::popupRect` and `popupRowRect` report where the open lists were laid out. Additions: a list that fits is laid out and
  drawn as before.

- 2026-10-31: a folder chooser in the file dialog (`FileDialogOptions::folder`: zenity, kdialog, Windows shell). Additions: files dialogs are unchanged.

- 2026-10-31: file drops on X11 (XDND: `WindowOptions::acceptFiles` now works on Linux as on Windows), several files per open
  dialog (`FileDialogOptions::multiple`, `runFileDialogs` on Windows; `FileDialogProcess::poll` returns one path per line), and
  the last folder per dialog kind (`FileDialogOptions::key`, `lastDirectory`, `rememberDirectory`; kept in
  `spux_dialog_folders.txt` in the config folder). Additions: the single-file dialog and windows without `acceptFiles` are
  unchanged.

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
