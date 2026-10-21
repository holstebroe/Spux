# Controls: what exists, per flavor

Images are made by `spux_widget_gallery` (`tests/gallery/`) from the painters themselves, at their real size:

```
cmake --build build --target spux_widget_gallery
build/tests/spux_widget_gallery docs/widgets      # then optimize the PNGs if you like; the tool writes them uncompressed
```

A control is **done** in a flavor when its painter exists and is tested (every painter draws inside its bounds, in
`ui_classic_style_test` and `ui_modern_test`). The plugin decides what it uses (guidelines rule 1): a page that calls a widget
its flavor lacks fails to compile, and a plugin can always pass its own painter per call, draw its own widgets with
`Gui::view` and `Gui::interact`, and draw its own background in the root view.

Classic has three shipped looks: **slate** (a dark console), **silver** (worn silver paint, knurled knobs) and **forge**
(forged iron plates, copper trim, amber lettering, arcs around the knobs). All are `ClassicConfig` data.

## Standard widgets

Retro is not started: no Retro painter exists yet.

| Control | Call | Classic, slate | Classic, silver | Classic, forge | Modern | Retro | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Knob | `g.knob(id, rect, {…})` | <img src="widgets/classic_knob.png" width="84" alt="classic_knob"> | <img src="widgets/silver_knob.png" width="84" alt="silver_knob"> | <img src="widgets/forge_knob.png" width="84" alt="forge_knob"> | <img src="widgets/modern_knob.png" width="84" alt="modern_knob"> | not started | Rotary parameter control. Value and label rows; bipolar scale; typed value entry. Classic: disc or knurled, printed ticks or circular arcs (config). |
| Knob, modulated and pickable | `g.knob(…, {.pickable=true})` + the host's `ModulationHost` | <img src="widgets/classic_knob_modulated.png" width="84" alt="classic_knob_modulated"> | <img src="widgets/silver_knob_modulated.png" width="84" alt="silver_knob_modulated"> | <img src="widgets/forge_knob_modulated.png" width="84" alt="forge_knob_modulated"> | <img src="widgets/modern_knob_modulated.png" width="84" alt="modern_knob_modulated"> | not started | The live modulated value on its own arc, and a highlight while the knob can be picked (a modulation target, MIDI learn). Classic draws the arc in looks that have arcs (forge); Modern draws rings, voices and the depth handle. |
| Slider, vertical | `g.slider(id, rect, {.vertical=true})` | <img src="widgets/classic_slider_vertical.png" width="52" alt="classic_slider_vertical"> | <img src="widgets/silver_slider_vertical.png" width="52" alt="silver_slider_vertical"> | <img src="widgets/forge_slider_vertical.png" width="52" alt="forge_slider_vertical"> | <img src="widgets/modern_slider_vertical.png" width="52" alt="modern_slider_vertical"> | not started | Fader with scale. Jump or relative drag. |
| Slider, horizontal | `g.slider(id, rect)` | <img src="widgets/classic_slider_horizontal.png" width="150" alt="classic_slider_horizontal"> | <img src="widgets/silver_slider_horizontal.png" width="150" alt="silver_slider_horizontal"> | <img src="widgets/forge_slider_horizontal.png" width="150" alt="forge_slider_horizontal"> | <img src="widgets/modern_slider_horizontal.png" width="150" alt="modern_slider_horizontal"> | not started | Classic prints no label or value for the horizontal fader yet. |
| Toggle | `g.toggle(id, rect, {…})` | <img src="widgets/classic_toggle.png" width="110" alt="classic_toggle"> | <img src="widgets/silver_toggle.png" width="110" alt="silver_toggle"> | <img src="widgets/forge_toggle.png" width="110" alt="forge_toggle"> | <img src="widgets/modern_toggle.png" width="110" alt="modern_toggle"> | not started | Classic: rocker key (slate) or lever switch (silver, forge: with a lamp), chosen by the config. |
| Selector | `g.selector(id, rect, {.labels=…})` | <img src="widgets/classic_selector.png" width="150" alt="classic_selector"> | <img src="widgets/silver_selector.png" width="150" alt="silver_selector"> | <img src="widgets/forge_selector.png" width="150" alt="forge_selector"> | <img src="widgets/modern_selector.png" width="150" alt="modern_selector"> | not started | Segments for a few choices. |
| Selector, vertical | `g.selector(id, rect, {.vertical=true})` | <img src="widgets/classic_selector_vertical.png" width="90" alt="classic_selector_vertical"> | <img src="widgets/silver_selector_vertical.png" width="90" alt="silver_selector_vertical"> | <img src="widgets/forge_selector_vertical.png" width="90" alt="forge_selector_vertical"> | <img src="widgets/modern_selector_vertical.png" width="90" alt="modern_selector_vertical"> | not started | A lever in a slot (Classic) or a column of segments (Modern). |
| Selector with icons | `g.selector(id, rect, {.icons=…})` | <img src="widgets/classic_selector_icons.png" width="150" alt="classic_selector_icons"> | <img src="widgets/silver_selector_icons.png" width="150" alt="silver_selector_icons"> | <img src="widgets/forge_selector_icons.png" width="150" alt="forge_selector_icons"> | <img src="widgets/modern_selector_icons.png" width="150" alt="modern_selector_icons"> | not started | Icons replace the labels; any `Icon`, built-in or the plugin's own. |
| Button | `g.button(id, rect, label, {…})` | <img src="widgets/classic_button.png" width="90" alt="classic_button"> | <img src="widgets/silver_button.png" width="90" alt="silver_button"> | <img src="widgets/forge_button.png" width="90" alt="forge_button"> | <img src="widgets/modern_button.png" width="90" alt="modern_button"> | not started | Plain, on, hot, pressed. Classic also has the cap button (colored, two-line legend); Modern the chip. |
| Button with icon | `g.button(id, rect, label, {.icon=…})` | <img src="widgets/classic_button_icon.png" width="90" alt="classic_button_icon"> | <img src="widgets/silver_button_icon.png" width="90" alt="silver_button_icon"> | <img src="widgets/forge_button_icon.png" width="90" alt="forge_button_icon"> | <img src="widgets/modern_button_icon.png" width="90" alt="modern_button_icon"> | not started | An icon beside the label, or alone when the label is empty. |
| Button, arrows | `{.variant=kButtonArrowLeft/Right}` | <img src="widgets/classic_button_arrows.png" width="80" alt="classic_button_arrows"> | <img src="widgets/silver_button_arrows.png" width="80" alt="silver_button_arrows"> | <img src="widgets/forge_button_arrows.png" width="80" alt="forge_button_arrows"> | <img src="widgets/modern_button_arrows.png" width="80" alt="modern_button_arrows"> | not started | Previous and next; every flavor draws them (`StdButtonVariant`). |
| Button, display | `{.variant=kButtonDisplay}` | <img src="widgets/classic_button_display.png" width="170" alt="classic_button_display"> | <img src="widgets/silver_button_display.png" width="170" alt="silver_button_display"> | <img src="widgets/forge_button_display.png" width="170" alt="forge_button_display"> | <img src="widgets/modern_button_display.png" width="170" alt="modern_button_display"> | not started | A recessed display of a name that opens something; every flavor draws it. |
| LED | `g.led(id, rect, on, {…})` | <img src="widgets/classic_led.png" width="90" alt="classic_led"> | <img src="widgets/silver_led.png" width="90" alt="silver_led"> | <img src="widgets/forge_led.png" width="90" alt="forge_led"> | <img src="widgets/modern_led.png" width="90" alt="modern_led"> | not started | A lamp with a label and a level. No input. Its own painter interface (`LedPainter`). |
| Keyboard | `g.keyboard(id, rect, {…})` returns `KeyboardEvents` | <img src="widgets/classic_keyboard.png" width="300" alt="classic_keyboard"> | <img src="widgets/silver_keyboard.png" width="300" alt="silver_keyboard"> | <img src="widgets/forge_keyboard.png" width="300" alt="forge_keyboard"> | <img src="widgets/modern_keyboard.png" width="300" alt="modern_keyboard"> | not started | Piano keys `first..last`; reports press, release, velocity and glide; the plugin sends the notes. Sounding keys are shown from `lit`. |
| Tabs | `g.tabs(id, rect, labels, n, current)` returns the index | <img src="widgets/classic_tabs.png" width="200" alt="classic_tabs"> | <img src="widgets/silver_tabs.png" width="200" alt="silver_tabs"> | <img src="widgets/forge_tabs.png" width="200" alt="forge_tabs"> | <img src="widgets/modern_tabs.png" width="200" alt="modern_tabs"> | not started | A row of tabs; the selection is the page's state. |
| Readout | `g.readout(id, rect, {.idle=…})` | <img src="widgets/classic_readout.png" width="260" alt="classic_readout"> | <img src="widgets/silver_readout.png" width="260" alt="silver_readout"> | <img src="widgets/forge_readout.png" width="260" alt="forge_readout"> | <img src="widgets/modern_readout.png" width="260" alt="modern_readout"> | not started | Name, value and hint of the parameter being dragged or under the pointer. |
| Dropdown | `g.dropdown(id, rect, label)` or `{.drag=true}` | <img src="widgets/classic_dropdown.png" width="150" alt="classic_dropdown"> | <img src="widgets/silver_dropdown.png" width="150" alt="silver_dropdown"> | <img src="widgets/forge_dropdown.png" width="150" alt="forge_dropdown"> | <img src="widgets/modern_dropdown.png" width="150" alt="modern_dropdown"> | not started | Opens the popup list for an enum or integer parameter; with `drag` a vertical drag steps through the values and a click still opens the list. |
| Stepper | `g.stepper(id, rect, label)` | <img src="widgets/classic_stepper.png" width="150" alt="classic_stepper"> | <img src="widgets/silver_stepper.png" width="150" alt="silver_stepper"> | <img src="widgets/forge_stepper.png" width="150" alt="forge_stepper"> | <img src="widgets/modern_stepper.png" width="150" alt="modern_stepper"> | not started | [-] value [+]. |
| Text field | `g.textField(id, rect, text)` | <img src="widgets/classic_text_field.png" width="150" alt="classic_text_field"> | <img src="widgets/silver_text_field.png" width="150" alt="silver_text_field"> | <img src="widgets/forge_text_field.png" width="150" alt="forge_text_field"> | <img src="widgets/modern_text_field.png" width="150" alt="modern_text_field"> | not started | Modal single-line entry (preset names). |
| Panel / frame | `g.panel(rect, title)` | <img src="widgets/classic_panel_frame.png" width="150" alt="classic_panel_frame"> | <img src="widgets/silver_panel_frame.png" width="150" alt="silver_panel_frame"> | <img src="widgets/forge_panel_frame.png" width="150" alt="forge_panel_frame"> | <img src="widgets/modern_panel_frame.png" width="150" alt="modern_panel_frame"> | not started | Classic: a titleless panel is the surface (gradient or worn paint), a titled one a frame, or a forged plate with rivets (forge). Modern: glass panel. |
| Popup menu | `g.popup(…)` | <img src="widgets/classic_popup.png" width="150" alt="classic_popup"> | <img src="widgets/silver_popup.png" width="150" alt="silver_popup"> | <img src="widgets/forge_popup.png" width="150" alt="forge_popup"> | <img src="widgets/modern_popup.png" width="150" alt="modern_popup"> | not started | Lists with columns, separators and disabled items. Cascading submenus are decided (U9) but not built. |
| Tooltip | (automatic on parameters) | <img src="widgets/classic_tooltip.png" width="190" alt="classic_tooltip"> | <img src="widgets/silver_tooltip.png" width="190" alt="silver_tooltip"> | <img src="widgets/forge_tooltip.png" width="190" alt="forge_tooltip"> | <img src="widgets/modern_tooltip.png" width="190" alt="modern_tooltip"> | not started | Shown after hovering a parameter. |

The **preset bar** (`presetBar(gui, id, rect)`, `spux/ui/PresetBar.hpp`) is not a painter: it is arrow buttons, a display button and
a list popup arranged over the host's `PresetHost`, so it looks like whichever flavor draws it. It reports SAVE; naming a
preset is the plugin's page.

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
| Level meter bar (peak, VU, segments) | The data side exists (`LevelMeter`, `History` in `spux/ui/Live.hpp`); no painter in any flavor except Modern's stereo meter view. |
| Modulation display in slate and silver | The forge look draws arcs; slate and silver draw none unless their config turns `knob.arc` on. |
| Multi-target modulation control (drag a source onto a knob, set the depth on a ring, as in Vital) | Not built. The pieces are there: `ModMatrix` holds many routes per source, `ModulationHost` shows live values and the armed source's depth handle, `KnobState::pickable` highlights targets. |
| XY pad | Not started. |
| Envelope and filter-response editors | Not started; generic ones may come after the second plugin needs them (Plumeflow's stay in the plugin). |
| Preset browser | An optional target is planned (`spux_presetbrowser`, a placeholder); the preset bar covers the header. |
| Scroll area / long lists | Not started (popups scroll by columns only). |
| Cascading submenus in the popup | Decided (U9), not built. |
| Classic spectrum, traces, curve editor | Not started (Modern only). |
| Retro: every painter | Not started. |

## Plugin territory (not shared)

Widgets that belong to one plugin stay there, built on `Gui::view`, `Gui::interact` and `ShadedRenderer::graphics()`: for
example Tannhauser's ribbon, paddles, tone selector legends and LCD readout, Acidus's printed waveform symbols and logo plate,
Gritbaal's routing summary text, Plumeflow's attractor, envelope and signal-flow views.
