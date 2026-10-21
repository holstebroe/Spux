#include "spux/ui/retro/RetroConfig.hpp"

namespace spux::inline v1::ui::retro {

// The tracker look: a flat grey screen of bevelled keys, blue selections and a blue-black display, in the 8x8 face.
RetroConfig RetroConfig::tracker() {
    RetroConfig c;
    c.font = &fontPt2();
    c.titleFont = c.font;
    c.ink = 0xFF000000;
    c.inkDim = 0xFF444444;
    c.inkOn = 0xFF3344AA;
    c.valueInk = 0xFF000000;
    c.frameInk = 0xFF444444;
    c.surface = 0xFF888888;
    c.frame.etched = true;
    c.knob.shape = KnobShape::Flat;
    c.knob.ticks = false;
    c.knob.shadow = false;
    c.knob.startDeg = 135.0f;
    c.knob.sweepDeg = 270.0f;
    c.knob.topRow = 10;
    c.knob.bottomRow = 10;
    c.knob.bezel = 0xFF444444;
    c.knob.face = 0xFFAAAAAA;
    c.knob.pointer = 0xFF000000;
    c.knob.pointerWidth = 1;
    c.knob.pick = 0xFF00AA00;
    c.toggle = ToggleShape::Key;
    c.slider.slot = 0xFF222233;
    c.slider.slotEdge = 0xFF444444;
    c.slider.fill = 0xFF3344AA;
    c.slider.topRow = 10;
    c.slider.bottomRow = 10;
    c.button.face = 0xFFAAAAAA;
    c.button.faceOn = 0xFF3344AA;
    c.button.edge = 0;
    c.button.outline = false;
    c.button.light = 0xFFDDDDDD;
    c.button.dark = 0xFF444444;
    c.button.ink = 0xFF000000;
    c.button.inkOn = 0xFFFFFFFF;
    c.display.fill = 0xFF222233;
    c.display.ink = 0xFFCCCCFF;
    c.display.inkDim = 0xFF8888AA;
    c.display.sunken = true;
    c.ledEdge = 0xFF444444;
    c.popup = {0xFFAAAAAA, 0xFF444444, 0xFF000000, 0xFF666666, 0xFF3344AA, 0xFFFFFFFF};
    c.tooltip = {0xFFDDDDAA, 0xFF000000, 0xFF000000, 0xFF444444};
    c.keyboard.felt = 0xFF444444;
    Theme::Colors& k = c.theme.colors;
    k.background = c.surface;
    k.text = c.ink;
    k.textDim = c.inkDim;
    k.textFaint = 0xFF666666;
    k.panel = 0xFFAAAAAA;
    k.hairline = 0xFF444444;
    k.track = 0xFF222233;
    k.accent = 0xFF3344AA; // a widget without its own accent: the tracker's blue
    k.modulation = 0xFF007700;
    k.focus = 0xFFFFFF00;
    k.warn = 0xFFCC4400;
    k.popup = c.popup.fill;
    k.tooltip = c.tooltip.fill;
    c.theme.metrics = {8.0f, 8.0f, 8.0f, 8.0f, 12, 32, 2};
    return c;
}

} // namespace spux::inline v1::ui::retro
