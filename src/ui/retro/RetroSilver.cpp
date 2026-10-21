#include "spux/ui/retro/RetroConfig.hpp"

namespace spux::inline v1::ui::retro {

// The silver look: a light panel with outlined trims, silver fluted knobs with a printed scale, lever switches, dark
// readouts; the thin 5x7 face for names and the heavy 7-pixel face for panel titles.
RetroConfig RetroConfig::silver() {
    RetroConfig c;
    c.font = &font5x7();
    c.titleFont = &fontPanel7();
    c.trims.on = true;
    c.knob.pointerWidth = 3;
    Theme::Colors& k = c.theme.colors;
    k.background = c.surface;
    k.text = c.ink;
    k.textDim = c.inkDim;
    k.textFaint = 0xFF8A9096;
    k.panel = 0xFFC0C4C8;
    k.hairline = 0xFF808488;
    k.track = 0xFF1A1C1E;
    k.accent = 0xFF157A1C; // a widget without its own accent: no cap color; lit things are acid green
    k.modulation = 0xFFB0309A;
    k.focus = 0xFF2060E0;
    k.warn = 0xFFD06010;
    k.popup = c.popup.fill;
    k.tooltip = c.tooltip.fill;
    c.theme.metrics = {7.0f, 7.0f, 7.0f, 7.0f, 12, 32, 2};
    return c;
}

} // namespace spux::inline v1::ui::retro
