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

## Standard widgets

Retro is not started: no Retro painter exists yet.

| Control | Call | Classic, slate | Classic, silver | Modern | Retro | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| Knob | `g.knob(id, rect, {…})` | <img src="widgets/classic_knob.png" width="84" alt="classic_knob"> | <img src="widgets/silver_knob.png" width="84" alt="silver_knob"> | <img src="widgets/modern_knob.png" width="84" alt="modern_knob"> | not started | Rotary parameter control. Value and label rows; bipolar scale; typed value entry. Classic draws no modulation rings (Modern does). |
| Slider, vertical | `g.slider(id, rect, {.vertical=true})` | <img src="widgets/classic_slider_vertical.png" width="52" alt="classic_slider_vertical"> | <img src="widgets/silver_slider_vertical.png" width="52" alt="silver_slider_vertical"> | <img src="widgets/modern_slider_vertical.png" width="52" alt="modern_slider_vertical"> | not started | Fader with scale. Jump or relative drag. |
| Slider, horizontal | `g.slider(id, rect)` | <img src="widgets/classic_slider_horizontal.png" width="150" alt="classic_slider_horizontal"> | <img src="widgets/silver_slider_horizontal.png" width="150" alt="silver_slider_horizontal"> | <img src="widgets/modern_slider_horizontal.png" width="150" alt="modern_slider_horizontal"> | not started | Classic prints no label or value for the horizontal fader yet. |
| Toggle | `g.toggle(id, rect, {…})` | <img src="widgets/classic_toggle.png" width="110" alt="classic_toggle"> | <img src="widgets/silver_toggle.png" width="110" alt="silver_toggle"> | <img src="widgets/modern_toggle.png" width="110" alt="modern_toggle"> | not started | Classic: rocker key (slate) or lever switch (silver), chosen by the config. |
| Selector | `g.selector(id, rect, {.labels=…})` | <img src="widgets/classic_selector.png" width="150" alt="classic_selector"> | <img src="widgets/silver_selector.png" width="150" alt="silver_selector"> | <img src="widgets/modern_selector.png" width="150" alt="modern_selector"> | not started | Segments for a few choices. |
| Selector, vertical | `g.selector(id, rect, {.vertical=true})` | <img src="widgets/classic_selector_vertical.png" width="90" alt="classic_selector_vertical"> | <img src="widgets/silver_selector_vertical.png" width="90" alt="silver_selector_vertical"> | <img src="widgets/modern_selector_vertical.png" width="90" alt="modern_selector_vertical"> | not started | A lever in a slot (Classic) or a column of segments (Modern). |
| Selector with icons | `g.selector(id, rect, {.icons=…})` | <img src="widgets/classic_selector_icons.png" width="150" alt="classic_selector_icons"> | <img src="widgets/silver_selector_icons.png" width="150" alt="silver_selector_icons"> | <img src="widgets/modern_selector_icons.png" width="150" alt="modern_selector_icons"> | not started | Icons replace the labels; any `Icon`, built-in or the plugin's own. |
| Button | `g.button(id, rect, label, {…})` | <img src="widgets/classic_button.png" width="90" alt="classic_button"> | <img src="widgets/silver_button.png" width="90" alt="silver_button"> | <img src="widgets/modern_button.png" width="90" alt="modern_button"> | not started | Plain, on, hot, pressed. Classic also has the cap button (colored, two-line legend) and arrow buttons; Modern the chip. |
| Button with icon | `g.button(id, rect, label, {.icon=…})` | <img src="widgets/classic_button_icon.png" width="90" alt="classic_button_icon"> | <img src="widgets/silver_button_icon.png" width="90" alt="silver_button_icon"> | <img src="widgets/modern_button_icon.png" width="90" alt="modern_button_icon"> | not started | An icon beside the label, or alone when the label is empty. |
| LED | `g.led(id, rect, on, {…})` | <img src="widgets/classic_led.png" width="90" alt="classic_led"> | <img src="widgets/silver_led.png" width="90" alt="silver_led"> | <img src="widgets/modern_led.png" width="90" alt="modern_led"> | not started | A lamp with a label and a level. No input. Its own painter interface (`LedPainter`). |
| Keyboard | `g.keyboard(id, rect, {…})` returns `KeyboardEvents` | <img src="widgets/classic_keyboard.png" width="300" alt="classic_keyboard"> | <img src="widgets/silver_keyboard.png" width="300" alt="silver_keyboard"> | <img src="widgets/modern_keyboard.png" width="300" alt="modern_keyboard"> | not started | Piano keys `first..last`; reports press, release, velocity and glide; the plugin sends the notes. Sounding keys are shown from `lit`. |
| Dropdown | `g.dropdown(id, rect, label)` | <img src="widgets/classic_dropdown.png" width="150" alt="classic_dropdown"> | <img src="widgets/silver_dropdown.png" width="150" alt="silver_dropdown"> | <img src="widgets/modern_dropdown.png" width="150" alt="modern_dropdown"> | not started | Opens the popup list for an enum or integer parameter. |
| Stepper | `g.stepper(id, rect, label)` | <img src="widgets/classic_stepper.png" width="150" alt="classic_stepper"> | <img src="widgets/silver_stepper.png" width="150" alt="silver_stepper"> | <img src="widgets/modern_stepper.png" width="150" alt="modern_stepper"> | not started | [-] value [+]. |
| Text field | `g.textField(id, rect, text)` | <img src="widgets/classic_text_field.png" width="150" alt="classic_text_field"> | <img src="widgets/silver_text_field.png" width="150" alt="silver_text_field"> | <img src="widgets/modern_text_field.png" width="150" alt="modern_text_field"> | not started | Modal single-line entry (preset names). |
| Panel / frame | `g.panel(rect, title)` | <img src="widgets/classic_panel_frame.png" width="150" alt="classic_panel_frame"> | <img src="widgets/silver_panel_frame.png" width="150" alt="silver_panel_frame"> | <img src="widgets/modern_panel_frame.png" width="150" alt="modern_panel_frame"> | not started | Classic: a titleless panel is the surface (gradient or worn paint), a titled one a frame. Modern: glass panel. |
| Popup menu | `g.popup(…)` | <img src="widgets/classic_popup.png" width="150" alt="classic_popup"> | <img src="widgets/silver_popup.png" width="150" alt="silver_popup"> | <img src="widgets/modern_popup.png" width="150" alt="modern_popup"> | not started | Lists with columns, separators and disabled items. Cascading submenus are decided (U9) but not built. |
| Tooltip | (automatic on parameters) | <img src="widgets/classic_tooltip.png" width="190" alt="classic_tooltip"> | <img src="widgets/silver_tooltip.png" width="190" alt="silver_tooltip"> | <img src="widgets/modern_tooltip.png" width="190" alt="modern_tooltip"> | not started | Shown after hovering a parameter. |

## Icons

Waveforms nearly every oscillator wants are built in (`spux/ui/Icons.hpp`): sine, triangle, saw, saw down, square, pulse,
noise, sample and hold. They draw through the basics of `IRenderer`, so every flavor shows them. A plugin makes its own icon
with one function and passes it where the built-in ones go.

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
| Modulation display on a knob (rings, voices, depth handle) | Modern only; the painter state carries it, Classic draws none. |
| XY pad | Not started. |
| Tabs / page selector | Not started; a row of buttons does it today. |
| Envelope and filter-response editors | Not started; generic ones may come after the second plugin needs them (Plumeflow's stay in the plugin). |
| Preset browser | An optional target is planned (`spux_presetbrowser`, a placeholder). |
| Scroll area / long lists | Not started (popups scroll by columns only). |
| Cascading submenus in the popup | Decided (U9), not built. |
| Classic spectrum, traces, curve editor | Not started (Modern only). |
| Retro: every painter | Not started. |

## Plugin territory (not shared)

Widgets that belong to one plugin stay there, built on `Gui::view`, `Gui::interact` and `ShadedRenderer::graphics()`: for
example Tannhauser's ribbon, paddles, tone selector legends and LCD readout, Acidus's printed waveform symbols and logo plate,
Plumeflow's attractor, envelope and signal-flow views.
