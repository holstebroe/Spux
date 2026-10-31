# Controls: what exists, per flavor

Images are made by `spux_widget_gallery` (`tests/gallery/`) from the painters themselves, at their real size:

```
cmake --build build --target spux_widget_gallery
build/tests/spux_widget_gallery docs/widgets      # then optimize the PNGs if you like; the tool writes them uncompressed
```

A control is **done** in a flavor when its painter exists and is tested (every painter draws inside its bounds, in
`ui_classic_style_test` and `ui_modern_test`). The plugin decides what it uses (guidelines rule 1): a page that calls a widget
its flavor lacks fails to compile, and a plugin can always pass its own painter per call, draw its own widgets with
`Gui::view` and `Gui::interact` (`Gui::wheelIn` reads the wheel over an area without taking the press), and draw its own
background in the root view.

Classic has three shipped looks: **slate** (a dark console), **silver** (worn silver paint, knurled knobs) and **forge**
(forged iron plates, copper trim, amber lettering, arcs around the knobs). All are `ClassicConfig` data.

## Standard widgets

Retro has three shipped looks, all `RetroConfig` data: **silver** (a silver panel, fluted knobs with a printed scale, lever
switches, dark readouts: the Retro column below), **tracker** (a flat grey screen of bevelled keys and blue selections in the
8x8 tracker face) and **forge** (forged iron, copper outlines, amber lettering). Pages in the three looks, at twice their size:

<img src="widgets/retro_silver_page.png" width="480" alt="retro_silver_page"> <img src="widgets/retro_tracker_page.png" width="480" alt="retro_tracker_page"> <img src="widgets/retro_forge_page.png" width="480" alt="retro_forge_page">

Retro draws whole pixels only; the images are made at the real size and shown at 2x.

| Control | Call | Classic, slate | Classic, silver | Classic, forge | Modern | Retro | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Knob | `g.knob(id, rect, {…})` | <img src="widgets/classic_knob.png" width="84" alt="classic_knob"> | <img src="widgets/silver_knob.png" width="84" alt="silver_knob"> | <img src="widgets/forge_knob.png" width="84" alt="forge_knob"> | <img src="widgets/modern_knob.png" width="84" alt="modern_knob"> | <img src="widgets/retro_silver_knob.png" width="84" alt="retro_silver_knob"> | Rotary parameter control. Value and label rows; bipolar scale; typed value entry. Classic: disc or knurled, printed ticks or circular arcs (config). Retro: fluted with printed ticks (silver, forge) or a flat bevelled disc (tracker). |
| Knob, modulated and pickable | `g.knob(…, {.pickable=true})` + the host's `ModulationHost` | <img src="widgets/classic_knob_modulated.png" width="84" alt="classic_knob_modulated"> | <img src="widgets/silver_knob_modulated.png" width="84" alt="silver_knob_modulated"> | <img src="widgets/forge_knob_modulated.png" width="84" alt="forge_knob_modulated"> | <img src="widgets/modern_knob_modulated.png" width="84" alt="modern_knob_modulated"> | <img src="widgets/retro_silver_knob_modulated.png" width="84" alt="retro_silver_knob_modulated"> | The live modulated value on its own arc, and a highlight while the knob can be picked (a modulation target, MIDI learn). Classic draws the arc in looks that have arcs (forge); Modern draws rings, voices and the depth handle; Retro draws the swing between the knob's own value and the modulated one as a one-pixel arc just outside the dial, and the pick highlight as a ring. |
| Slider, vertical | `g.slider(id, rect, {.vertical=true})` | <img src="widgets/classic_slider_vertical.png" width="52" alt="classic_slider_vertical"> | <img src="widgets/silver_slider_vertical.png" width="52" alt="silver_slider_vertical"> | <img src="widgets/forge_slider_vertical.png" width="52" alt="forge_slider_vertical"> | <img src="widgets/modern_slider_vertical.png" width="52" alt="modern_slider_vertical"> | <img src="widgets/retro_silver_slider_vertical.png" width="52" alt="retro_silver_slider_vertical"> | Fader with scale (`slider.slotW`, `slotEnd` and `scaleBothSides` in `ClassicConfig`). Jump or relative drag. |
| Slider, horizontal | `g.slider(id, rect)` | <img src="widgets/classic_slider_horizontal.png" width="150" alt="classic_slider_horizontal"> | <img src="widgets/silver_slider_horizontal.png" width="150" alt="silver_slider_horizontal"> | <img src="widgets/forge_slider_horizontal.png" width="150" alt="forge_slider_horizontal"> | <img src="widgets/modern_slider_horizontal.png" width="150" alt="modern_slider_horizontal"> | <img src="widgets/retro_silver_slider_horizontal.png" width="150" alt="retro_silver_slider_horizontal"> | Classic prints no label or value for the horizontal fader yet; Retro prints them above and below the slot when the bounds leave a text row. |
| Toggle | `g.toggle(id, rect, {…})` | <img src="widgets/classic_toggle.png" width="110" alt="classic_toggle"> | <img src="widgets/silver_toggle.png" width="110" alt="silver_toggle"> | <img src="widgets/forge_toggle.png" width="110" alt="forge_toggle"> | <img src="widgets/modern_toggle.png" width="110" alt="modern_toggle"> | <img src="widgets/retro_silver_toggle.png" width="110" alt="retro_silver_toggle"> | Classic: rocker key (slate) or lever switch (silver, forge: with a lamp), chosen by the config. Retro: a lever in a slot (silver, forge) or a bevelled key with a lamp (tracker). |
| Selector | `g.selector(id, rect, {.labels=…})` | <img src="widgets/classic_selector.png" width="150" alt="classic_selector"> | <img src="widgets/silver_selector.png" width="150" alt="silver_selector"> | <img src="widgets/forge_selector.png" width="150" alt="forge_selector"> | <img src="widgets/modern_selector.png" width="150" alt="modern_selector"> | <img src="widgets/retro_silver_selector.png" width="150" alt="retro_silver_selector"> | Segments for a few choices. |
| Selector, vertical | `g.selector(id, rect, {.vertical=true})` | <img src="widgets/classic_selector_vertical.png" width="90" alt="classic_selector_vertical"> | <img src="widgets/silver_selector_vertical.png" width="90" alt="silver_selector_vertical"> | <img src="widgets/forge_selector_vertical.png" width="90" alt="forge_selector_vertical"> | <img src="widgets/modern_selector_vertical.png" width="90" alt="modern_selector_vertical"> | <img src="widgets/retro_silver_selector_vertical.png" width="90" alt="retro_silver_selector_vertical"> | A lever in a slot (Classic) or a column of segments (Modern). |
| Selector with icons | `g.selector(id, rect, {.icons=…})` | <img src="widgets/classic_selector_icons.png" width="150" alt="classic_selector_icons"> | <img src="widgets/silver_selector_icons.png" width="150" alt="silver_selector_icons"> | <img src="widgets/forge_selector_icons.png" width="150" alt="forge_selector_icons"> | <img src="widgets/modern_selector_icons.png" width="150" alt="modern_selector_icons"> | <img src="widgets/retro_silver_selector_icons.png" width="150" alt="retro_silver_selector_icons"> | Icons replace the labels; any `Icon`, built-in or the plugin's own. |
| Button | `g.button(id, rect, label, {…})` | <img src="widgets/classic_button.png" width="90" alt="classic_button"> | <img src="widgets/silver_button.png" width="90" alt="silver_button"> | <img src="widgets/forge_button.png" width="90" alt="forge_button"> | <img src="widgets/modern_button.png" width="90" alt="modern_button"> | <img src="widgets/retro_silver_button.png" width="90" alt="retro_silver_button"> | Plain, on, hot, pressed. Classic also has the cap button (colored, two-line legend); Modern the chip. |
| Button with icon | `g.button(id, rect, label, {.icon=…})` | <img src="widgets/classic_button_icon.png" width="90" alt="classic_button_icon"> | <img src="widgets/silver_button_icon.png" width="90" alt="silver_button_icon"> | <img src="widgets/forge_button_icon.png" width="90" alt="forge_button_icon"> | <img src="widgets/modern_button_icon.png" width="90" alt="modern_button_icon"> | <img src="widgets/retro_silver_button_icon.png" width="90" alt="retro_silver_button_icon"> | An icon beside the label, or alone when the label is empty. |
| Button, arrows | `{.variant=kButtonArrowLeft/Right}` | <img src="widgets/classic_button_arrows.png" width="80" alt="classic_button_arrows"> | <img src="widgets/silver_button_arrows.png" width="80" alt="silver_button_arrows"> | <img src="widgets/forge_button_arrows.png" width="80" alt="forge_button_arrows"> | <img src="widgets/modern_button_arrows.png" width="80" alt="modern_button_arrows"> | <img src="widgets/retro_silver_button_arrows.png" width="80" alt="retro_silver_button_arrows"> | Previous and next; every flavor draws them (`StdButtonVariant`). |
| Button, display | `{.variant=kButtonDisplay}` | <img src="widgets/classic_button_display.png" width="170" alt="classic_button_display"> | <img src="widgets/silver_button_display.png" width="170" alt="silver_button_display"> | <img src="widgets/forge_button_display.png" width="170" alt="forge_button_display"> | <img src="widgets/modern_button_display.png" width="170" alt="modern_button_display"> | <img src="widgets/retro_silver_button_display.png" width="170" alt="retro_silver_button_display"> | A recessed display of a name that opens something; every flavor draws it. |
| LED | `g.led(id, rect, on, {…})` | <img src="widgets/classic_led.png" width="90" alt="classic_led"> | <img src="widgets/silver_led.png" width="90" alt="silver_led"> | <img src="widgets/forge_led.png" width="90" alt="forge_led"> | <img src="widgets/modern_led.png" width="90" alt="modern_led"> | <img src="widgets/retro_silver_led.png" width="90" alt="retro_silver_led"> | A lamp with a label and a level. No input. Its own painter interface (`LedPainter`). |
| Keyboard | `g.keyboard(id, rect, {…})` returns `KeyboardEvents` | <img src="widgets/classic_keyboard.png" width="300" alt="classic_keyboard"> | <img src="widgets/silver_keyboard.png" width="300" alt="silver_keyboard"> | <img src="widgets/forge_keyboard.png" width="300" alt="forge_keyboard"> | <img src="widgets/modern_keyboard.png" width="300" alt="modern_keyboard"> | <img src="widgets/retro_silver_keyboard.png" width="300" alt="retro_silver_keyboard"> | Piano keys `first..last`; reports press, release, velocity and glide; the plugin sends the notes. Sounding keys are shown from `lit`. |
| Tabs | `g.tabs(id, rect, labels, n, current)` returns the index | <img src="widgets/classic_tabs.png" width="200" alt="classic_tabs"> | <img src="widgets/silver_tabs.png" width="200" alt="silver_tabs"> | <img src="widgets/forge_tabs.png" width="200" alt="forge_tabs"> | <img src="widgets/modern_tabs.png" width="200" alt="modern_tabs"> | <img src="widgets/retro_silver_tabs.png" width="200" alt="retro_silver_tabs"> | A row of tabs; the selection is the page's state. |
| Readout | `g.readout(id, rect, {.idle=…})` | <img src="widgets/classic_readout.png" width="260" alt="classic_readout"> | <img src="widgets/silver_readout.png" width="260" alt="silver_readout"> | <img src="widgets/forge_readout.png" width="260" alt="forge_readout"> | <img src="widgets/modern_readout.png" width="260" alt="modern_readout"> | <img src="widgets/retro_silver_readout.png" width="260" alt="retro_silver_readout"> | Name, value and hint of the parameter being dragged or under the pointer. |
| Dropdown | `g.dropdown(id, rect, label)` or `{.drag=true}` | <img src="widgets/classic_dropdown.png" width="150" alt="classic_dropdown"> | <img src="widgets/silver_dropdown.png" width="150" alt="silver_dropdown"> | <img src="widgets/forge_dropdown.png" width="150" alt="forge_dropdown"> | <img src="widgets/modern_dropdown.png" width="150" alt="modern_dropdown"> | <img src="widgets/retro_silver_dropdown.png" width="150" alt="retro_silver_dropdown"> | Opens the popup list for an enum or integer parameter; with `drag` a vertical drag steps through the values and a click still opens the list. |
| Stepper | `g.stepper(id, rect, label)` | <img src="widgets/classic_stepper.png" width="150" alt="classic_stepper"> | <img src="widgets/silver_stepper.png" width="150" alt="silver_stepper"> | <img src="widgets/forge_stepper.png" width="150" alt="forge_stepper"> | <img src="widgets/modern_stepper.png" width="150" alt="modern_stepper"> | <img src="widgets/retro_silver_stepper.png" width="150" alt="retro_silver_stepper"> | [-] value [+]. |
| Text field | `g.textField(id, rect, text)` | <img src="widgets/classic_text_field.png" width="150" alt="classic_text_field"> | <img src="widgets/silver_text_field.png" width="150" alt="silver_text_field"> | <img src="widgets/forge_text_field.png" width="150" alt="forge_text_field"> | <img src="widgets/modern_text_field.png" width="150" alt="modern_text_field"> | <img src="widgets/retro_silver_text_field.png" width="150" alt="retro_silver_text_field"> | Modal single-line entry (preset names). |
| Panel / frame | `g.panel(rect, title)` | <img src="widgets/classic_panel_frame.png" width="150" alt="classic_panel_frame"> | <img src="widgets/silver_panel_frame.png" width="150" alt="silver_panel_frame"> | <img src="widgets/forge_panel_frame.png" width="150" alt="forge_panel_frame"> | <img src="widgets/modern_panel_frame.png" width="150" alt="modern_panel_frame"> | <img src="widgets/retro_silver_panel_frame.png" width="150" alt="retro_silver_panel_frame"> | Classic: a titleless panel is the surface (gradient, brushed steel or worn paint), a titled one a frame, or a forged plate with rivets (forge). Modern: glass panel. |
| Popup menu | `g.popup(…)` | <img src="widgets/classic_popup.png" width="150" alt="classic_popup"> | <img src="widgets/silver_popup.png" width="150" alt="silver_popup"> | <img src="widgets/forge_popup.png" width="150" alt="forge_popup"> | <img src="widgets/modern_popup.png" width="150" alt="modern_popup"> | <img src="widgets/retro_silver_popup.png" width="150" alt="retro_silver_popup"> | Lists with columns, separators and disabled items, and submenus: a row with `PopupItem::children` opens its list beside it under the pointer, to any depth, and `popupPath(…, choose(path))` reports the route. |
| Tooltip | (automatic on parameters) | <img src="widgets/classic_tooltip.png" width="190" alt="classic_tooltip"> | <img src="widgets/silver_tooltip.png" width="190" alt="silver_tooltip"> | <img src="widgets/forge_tooltip.png" width="190" alt="forge_tooltip"> | <img src="widgets/modern_tooltip.png" width="190" alt="modern_tooltip"> | <img src="widgets/retro_silver_tooltip.png" width="190" alt="retro_silver_tooltip"> | Shown after hovering a parameter. |

The **preset bar** (`presetBar(gui, id, rect)`, `spux/ui/PresetBar.hpp`) is not a painter: it is arrow buttons, a display button and
a list popup arranged over the host's `PresetHost`, so it looks like whichever flavor draws it. It reports SAVE; naming a
preset is the plugin's page.

## Retro looks

The same controls in the other two Retro looks (the silver look is the Retro column above). Each look is a `RetroConfig`:
`RetroConfig::silver()`, `tracker()` and `forge()`; a plugin starts from one and changes what it wants (colors, fonts, knob and
toggle shapes, bevels, the rows around a knob). Each look lives in its own object, so a program links only the faces of the look it uses.

| Control | Retro, tracker | Retro, forge |
| --- | --- | --- |
| Knob | <img src="widgets/retro_tracker_knob.png" width="84" alt="retro_tracker_knob"> | <img src="widgets/retro_forge_knob.png" width="84" alt="retro_forge_knob"> |
| Knob, modulated and pickable | <img src="widgets/retro_tracker_knob_modulated.png" width="84" alt="retro_tracker_knob_modulated"> | <img src="widgets/retro_forge_knob_modulated.png" width="84" alt="retro_forge_knob_modulated"> |
| Slider, vertical | <img src="widgets/retro_tracker_slider_vertical.png" width="52" alt="retro_tracker_slider_vertical"> | <img src="widgets/retro_forge_slider_vertical.png" width="52" alt="retro_forge_slider_vertical"> |
| Slider, horizontal | <img src="widgets/retro_tracker_slider_horizontal.png" width="150" alt="retro_tracker_slider_horizontal"> | <img src="widgets/retro_forge_slider_horizontal.png" width="150" alt="retro_forge_slider_horizontal"> |
| Toggle | <img src="widgets/retro_tracker_toggle.png" width="110" alt="retro_tracker_toggle"> | <img src="widgets/retro_forge_toggle.png" width="110" alt="retro_forge_toggle"> |
| Selector | <img src="widgets/retro_tracker_selector.png" width="150" alt="retro_tracker_selector"> | <img src="widgets/retro_forge_selector.png" width="150" alt="retro_forge_selector"> |
| Selector, vertical | <img src="widgets/retro_tracker_selector_vertical.png" width="90" alt="retro_tracker_selector_vertical"> | <img src="widgets/retro_forge_selector_vertical.png" width="90" alt="retro_forge_selector_vertical"> |
| Selector with icons | <img src="widgets/retro_tracker_selector_icons.png" width="150" alt="retro_tracker_selector_icons"> | <img src="widgets/retro_forge_selector_icons.png" width="150" alt="retro_forge_selector_icons"> |
| Button | <img src="widgets/retro_tracker_button.png" width="90" alt="retro_tracker_button"> | <img src="widgets/retro_forge_button.png" width="90" alt="retro_forge_button"> |
| Button with icon | <img src="widgets/retro_tracker_button_icon.png" width="90" alt="retro_tracker_button_icon"> | <img src="widgets/retro_forge_button_icon.png" width="90" alt="retro_forge_button_icon"> |
| Button, arrows | <img src="widgets/retro_tracker_button_arrows.png" width="80" alt="retro_tracker_button_arrows"> | <img src="widgets/retro_forge_button_arrows.png" width="80" alt="retro_forge_button_arrows"> |
| Button, display | <img src="widgets/retro_tracker_button_display.png" width="170" alt="retro_tracker_button_display"> | <img src="widgets/retro_forge_button_display.png" width="170" alt="retro_forge_button_display"> |
| LED | <img src="widgets/retro_tracker_led.png" width="90" alt="retro_tracker_led"> | <img src="widgets/retro_forge_led.png" width="90" alt="retro_forge_led"> |
| Keyboard | <img src="widgets/retro_tracker_keyboard.png" width="300" alt="retro_tracker_keyboard"> | <img src="widgets/retro_forge_keyboard.png" width="300" alt="retro_forge_keyboard"> |
| Tabs | <img src="widgets/retro_tracker_tabs.png" width="200" alt="retro_tracker_tabs"> | <img src="widgets/retro_forge_tabs.png" width="200" alt="retro_forge_tabs"> |
| Readout | <img src="widgets/retro_tracker_readout.png" width="260" alt="retro_tracker_readout"> | <img src="widgets/retro_forge_readout.png" width="260" alt="retro_forge_readout"> |
| Dropdown | <img src="widgets/retro_tracker_dropdown.png" width="150" alt="retro_tracker_dropdown"> | <img src="widgets/retro_forge_dropdown.png" width="150" alt="retro_forge_dropdown"> |
| Stepper | <img src="widgets/retro_tracker_stepper.png" width="150" alt="retro_tracker_stepper"> | <img src="widgets/retro_forge_stepper.png" width="150" alt="retro_forge_stepper"> |
| Text field | <img src="widgets/retro_tracker_text_field.png" width="150" alt="retro_tracker_text_field"> | <img src="widgets/retro_forge_text_field.png" width="150" alt="retro_forge_text_field"> |
| Panel / frame | <img src="widgets/retro_tracker_panel_frame.png" width="150" alt="retro_tracker_panel_frame"> | <img src="widgets/retro_forge_panel_frame.png" width="150" alt="retro_forge_panel_frame"> |
| Popup menu | <img src="widgets/retro_tracker_popup.png" width="150" alt="retro_tracker_popup"> | <img src="widgets/retro_forge_popup.png" width="150" alt="retro_forge_popup"> |
| Tooltip | <img src="widgets/retro_tracker_tooltip.png" width="190" alt="retro_tracker_tooltip"> | <img src="widgets/retro_forge_tooltip.png" width="190" alt="retro_forge_tooltip"> |

## Icons

Waveforms nearly every oscillator wants are built in (`spux/ui/Icons.hpp`, the `Wave` enum in `spux/core/Wave.hpp`, shared with
the LFO): sine, triangle, saw, saw down, square, pulse, noise, sample and hold. They draw through the basics of `IRenderer`, so
every flavor shows them. A plugin makes its own icon with one function and passes it where the built-in ones go.

<img src="widgets/core_waves.png" width="352" alt="waveform icons">

## Views (Modern only so far)

| View | Call | Classic | Modern | Retro |
| --- | --- | --- | --- | --- |
| Spectrum | `drawSpectrum(style, g, …)` | not started | <img src="widgets/modern_spectrum.png" width="200" alt="modern_spectrum"> | not started |
| Traces | `drawTraces(style, g, …)` | not started | <img src="widgets/modern_traces.png" width="200" alt="modern_traces"> | not started |
| Stereo meter | `drawStereoMeter(style, g, …)` | not started | <img src="widgets/modern_stereo_meter.png" width="200" alt="modern_stereo_meter"> | not started |
| Curve editor | `drawCurveEditor(style, g, …)` | not started | <img src="widgets/modern_curve_editor.png" width="200" alt="modern_curve_editor"> | not started |

## Not implemented

| Control | Where it stands |
| --- | --- |
| Level meter bar (peak, VU, segments) | The data side exists (`LevelMeter`, `History` in `spux/ui/Live.hpp`); no painter in any flavor except Modern's stereo meter view. `PixelRenderer::drawMeter` draws a column of segments for a plugin's own view. |
| Modulation display in slate and silver | The forge look draws arcs; slate and silver draw none unless their config turns `knob.arc` on. |
| Multi-target modulation control (drag a source onto a knob, set the depth on a ring, as in Vital) | Not built. The pieces are there: `ModMatrix` holds many routes per source, `ModulationHost` shows live values and the armed source's depth handle, `KnobState::pickable` highlights targets. |
| XY pad | Not started. |
| Envelope and filter-response editors | Not started; generic ones may come after the second plugin needs them (Plumeflow's stay in the plugin). |
| Preset browser | An optional target is planned (`spux_presetbrowser`, a placeholder); the preset bar covers the header. |
| Scroll area / long lists | Built for popups in all three flavors: lists taller than the view height scroll by windowing rows, with mouse wheel, keyboard, up/down buttons and scroll marker. |
| Cascading submenus in the popup | Built in all three flavors (U9): a chevron marks the row, the list opens beside it (to the left at the edge of the view); no keyboard navigation of submenus yet. |
| Retro in whole-step scales (U11) | Not built: the editor draws a CPU flavor at its logical size, so a Retro plugin that wants 2x or 3x scales the blitted frame itself. |
| Retro spectrum, traces and curve editor | Not started (Modern only). |
| Classic spectrum, traces, curve editor | Not started (Modern only). |

## Plugin territory (not shared)

Widgets that belong to one plugin stay there, built on `Gui::view`, `Gui::interact` and `ShadedRenderer::graphics()`: for
example Tannhauser's ribbon, paddles, tone selector legends and LCD readout, Acidus's printed waveform symbols and logo plate,
Gritbaal's routing summary text, Plumeflow's attractor, envelope and signal-flow views.
