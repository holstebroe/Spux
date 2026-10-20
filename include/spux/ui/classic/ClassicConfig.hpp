// The Classic flavor's look as data (guidelines 7, "Painters and themes"): which shapes the painters draw,
// the printed ink, the panel surface and the metrics of the controls. Two looks ship, slate() (a blue-grey
// console with dark caps and a soft diagonal gradient) and silver() (worn silver paint, knurled metal
// knobs, a lever switch in an escutcheon); a plugin starts from one and changes what it wants.
#pragma once

#include <cstdint>

#include "spux/ui/Theme.hpp"
#include "spux/ui/classic/ClassicDraw.hpp"

namespace spux::inline v1::ui::classic {

// The look of a knob: a dark disc with a pointer, or a knurled metal knob (foot ring, grip, chamfer, a top
// that takes a cap color).
enum class KnobShape { Disc, Knurled };
// The look of a toggle: a rocker key with a lamp, or a lever in a slot.
enum class ToggleShape { Rocker, Lever };
// What a panel without a title is: a soft gradient with a grain, or worn silver paint.
enum class PanelSurface { Slate, Worn };

// Values of ButtonState::variant that ClassicStyle draws.
enum ButtonVariant : int {
    kButtonPlain = 0,      // a dark rounded button with a light edge; a bright edge when on
    kButtonCap = 1,        // a colored cap (the accent), lit when on; a label with a newline is two lines
    kButtonArrowLeft = 2,  // a plain button with a chevron
    kButtonArrowRight = 3, //
    kButtonLed = 4         // a lamp (the accent, lit when on) with the label to its right
};

struct ClassicConfig {
    Theme theme;

    // Printed lettering: ink for names, dim ink for scales and units, the lines of a frame, the lit value text,
    // and the ink of a selected step or an active button.
    Argb ink = 0xF2E9E8E2, inkDim = 0xC0A8AAA6, frameInk = 0xB08E908C, valueInk = 0xFFFFD58A, inkOn = 0xFFFFFFFF;
    float wear = 0.08f;                                      // patchiness of the ink (0..1)
    float labelCap = 6.0f, valueCap = 8.0f, titleCap = 6.5f; // cap heights in logical pixels

    PanelSurface surface = PanelSurface::Slate;
    bool trims = false; // Worn: the darker extrusions along the top and bottom edges
    Color surfaceLight{0.205f, 0.225f, 0.245f}, surfaceDark{0.07f, 0.077f, 0.086f}; // Slate: top left to bottom right

    struct Knob {
        KnobShape shape = KnobShape::Disc;
        bool ticks = true; // the printed scale of 11 ticks around the dial
        // Rows above and below the dial for the value text and the label; 0 when the page prints its own.
        // Fixed by the config, not by the state, so the dial stays where the interaction expects it.
        float topRow = 12.0f, bottomRow = 10.0f;
        bool labelBelow = true; // the label goes in the bottom row and the value in the top row; else the reverse
        bool showValue = true;  // the value text is drawn at all
    } knob;

    ToggleShape toggle = ToggleShape::Rocker;

    struct Slider {
        float capW = 21.0f, capH = 11.0f;
    } slider;

    struct Button {
        Color top{0.215f, 0.225f, 0.24f}, bottom{0.125f, 0.13f, 0.14f}; // face, top to bottom
        Color edge{0.30f, 0.315f, 0.33f}, edgeOn{0.88f, 0.90f, 0.92f};  // outline idle and on
        float radius = 4.0f;
    } button;

    // The slate look: a dark console, soft gradient and grain, dark caps and buttons, printed in white.
    static ClassicConfig slate();
    // The silver look: worn silver paint, black ink, knurled metal knobs, lever switches.
    static ClassicConfig silver();
};

inline ClassicConfig ClassicConfig::slate() {
    ClassicConfig c;
    Theme::Colors& k = c.theme.colors;
    k.background = rgb(24, 26, 29);
    k.text = c.ink;
    k.textDim = c.inkDim;
    k.textFaint = c.frameInk;
    k.accent = rgb(184, 188, 192); // a widget without its own accent: neutral metal
    k.focus = rgb(230, 235, 240);
    k.track = rgb(0, 0, 0, 120);
    k.hairline = rgb(255, 255, 255, 40);
    c.theme.metrics.labelPx = 8.5f;
    c.theme.metrics.valuePx = 11.0f;
    c.theme.metrics.titlePx = 9.0f;
    return c;
}

inline ClassicConfig ClassicConfig::silver() {
    ClassicConfig c;
    c.ink = 0xF0222326;
    c.inkDim = 0xC0707478;
    c.frameInk = 0xC0303234;
    c.wear = 0.18f;
    c.labelCap = 7.0f;
    c.surface = PanelSurface::Worn;
    c.trims = true;
    c.knob.shape = KnobShape::Knurled;
    c.knob.topRow = 15.0f;
    c.knob.bottomRow = 0.0f;
    c.inkOn = 0xFF157A1C;
    c.knob.labelBelow = false;
    c.knob.showValue = false;
    c.toggle = ToggleShape::Lever;
    c.button.top = {0.80f, 0.81f, 0.82f};
    c.button.bottom = {0.58f, 0.59f, 0.60f};
    c.button.edge = {0.38f, 0.39f, 0.40f};
    c.button.edgeOn = {0.10f, 0.45f, 0.14f};
    Theme::Colors& k = c.theme.colors;
    k.background = rgb(200, 201, 198);
    k.text = c.ink;
    k.textDim = c.inkDim;
    k.textFaint = rgb(150, 152, 154);
    k.accent = rgb(200, 202, 205);
    k.focus = rgb(21, 122, 28);
    k.track = rgb(0, 0, 0, 90);
    k.hairline = rgb(0, 0, 0, 50);
    c.theme.metrics.labelPx = 9.5f;
    c.theme.metrics.valuePx = 11.0f;
    c.theme.metrics.titlePx = 9.5f;
    return c;
}

} // namespace spux::inline v1::ui::classic
