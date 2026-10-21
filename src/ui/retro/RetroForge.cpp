#include "spux/ui/retro/RetroConfig.hpp"

namespace spux::inline v1::ui::retro {

// The forge look: forged iron plates on a dark ground, copper trim and outlines, amber lettering, cyan for what is
// live; the same silver knobs and levers as silver().
RetroConfig RetroConfig::forge() {
    RetroConfig c;
    c.font = &font5x7();
    c.titleFont = &fontPanel7();
    c.ink = 0xFFD89A40;
    c.inkDim = 0xFF8A6A3A;
    c.inkOn = 0xFFFFCC00;
    c.valueInk = 0xFFFFCC00;
    c.frameInk = 0xFF8C5224;
    c.surface = 0xFF141517;
    c.frame.plate = true;
    c.frame.titleInk = 0xFFFF8A00;
    c.knob.shadow = false;
    c.knob.startDeg = 135.0f;
    c.knob.sweepDeg = 270.0f;
    c.knob.tick = 0xFF8C5224;
    c.knob.pointer = 0xFF101010;
    c.knob.topRow = 10;
    c.slider.fill = 0xFFFF8A00;
    c.slider.slot = 0xFF0A0C0D;
    c.slider.slotEdge = 0xFF8C5224;
    // dark keys with a copper outline, amber lettering, a lit key in amber
    c.button.face = 0xFF101214;
    c.button.faceOn = 0xFF281E14;
    c.button.edge = 0xFF8C5224;
    c.button.light = 0xFF3D4145;
    c.button.dark = 0xFF050607;
    c.button.ink = 0xFFD89A40;
    c.button.inkOn = 0xFFFFCC00;
    c.display.fill = 0xFF0E1012;
    c.display.edge = 0xFF8C5224;
    c.display.ink = 0xFFFFCC00;
    c.display.inkDim = 0xFFD89A40;
    c.ledEdge = 0xFF202020;
    c.popup = {0xFF101214, 0xFF8C5224, 0xFFD89A40, 0xFF6A5230, 0xFF281E14, 0xFFFFCC00};
    c.tooltip = {0xFF0A0C0D, 0xFF8C5224, 0xFF00E5FF, 0xFFD89A40};
    c.keyboard.felt = 0xFF8C5224;
    Theme::Colors& k = c.theme.colors;
    k.background = c.surface;
    k.text = c.ink;
    k.textDim = c.inkDim;
    k.textFaint = 0xFF3D4145;
    k.panel = 0xFF202326;
    k.hairline = 0xFF3D4145;
    k.track = 0xFF0A0C0D;
    k.accent = 0xFF00E5FF;
    k.modulation = 0xFF00E5FF;
    k.focus = 0xFF00E5FF;
    k.warn = 0xFFFF4500;
    k.popup = c.popup.fill;
    k.tooltip = c.tooltip.fill;
    c.theme.metrics = {7.0f, 7.0f, 7.0f, 7.0f, 12, 32, 2};
    return c;
}

} // namespace spux::inline v1::ui::retro
