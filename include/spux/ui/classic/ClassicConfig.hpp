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
    kButtonPlain = 0, // a dark rounded button with a light edge; a bright edge when on
    kButtonCap = 1    // a colored cap (the accent), lit when on; a label with a newline is two lines
}; // the arrows and the display button are core variants (StdButtonVariant, Painters.hpp)

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
        float startDeg = 120.0f, sweepDeg = 300.0f; // the dial: 7 o'clock, clockwise (degrees, 0 = 3 o'clock)

        // Circular indicators around the dial (instead of, or with, the printed ticks): a track, the value arc (from
        // the center for a bipolar knob), the live modulated value on an arc outside it, ticks at the ends, and the
        // highlight of a pickable knob. The dial's room grows to hold them.
        struct Arc {
            bool on = false;
            float gap = 5.0f, width = 2.0f,
                  modGap = 3.0f; // distance from the dial, thickness, step out for the mod arc
            Argb track = 0xFF3A2010, value = 0xFFFF4500, mod = 0xFF00E5FF, endTick = 0xFF8C5224,
                 centerTick = 0xFFFF8A00, pick = 0xFF00E5FF;
        } arc;

        // The knurled knob's metal: albedo of the foot ring, the grip, the chamfer and the top (silver by default);
        // a pointer color draws a glowing pointer line on the top instead of the incised one, a bezel color a ring
        // around the foot.
        Color metalFoot{0.15f, 0.15f, 0.16f}, metalGrip{0.62f, 0.63f, 0.65f}, metalChamfer{0.78f, 0.79f, 0.80f},
            metalTop{0.80f, 0.81f, 0.82f};
        Argb pointer = 0, pointerTip = 0xFFFFCC00, bezel = 0;
    } knob;

    ToggleShape toggle = ToggleShape::Rocker;
    // The lever switch: plate and handle albedo, and a lamp beside it that is lit when the switch is on.
    struct Lever {
        Color plate{0.74f, 0.75f, 0.76f}, handle{0.86f, 0.87f, 0.88f};
        bool lamp = false;
    } lever;

    // The frame of a titled panel: an outline with the title cut into it, or a forged plate (a dark gradient, a
    // trim line inside the edge, rivets in the corners and the title on a plate on the top edge).
    struct Frame {
        bool plate = false;
        Argb trim = 0xFF8C5224, titleInk = 0; // 0: the ink
        Color fillTop{0.125f, 0.137f, 0.149f}, fillBottom{0.07f, 0.078f, 0.086f};
        bool rivets = true;
    } frame;

    struct Slider {
        float capW = 21.0f, capH = 11.0f;
        float slotW = 5.0f;          // the groove's width
        float slotEnd = 3.0f;        // how far the groove reaches beyond the cap's travel at each end
        bool scaleBothSides = false; // the printed ticks on both sides of a vertical groove (else on the left)
    } slider;

    // The recessed display of a dropdown, a stepper's value and a text field: a dark glass with a light edge.
    struct Display {
        Color top{0.045f, 0.05f, 0.055f}, bottom{0.085f, 0.09f, 0.10f}, edge{0.30f, 0.315f, 0.33f};
        Argb ink = 0xFFF0F2F3; // the text on it
        float radius = 4.0f;
    } display;

    // The felt strip along the top of a keyboard, behind the keys.
    Argb keyboardFelt = 0xFF5A1E1A;

    struct Button {
        Color top{0.215f, 0.225f, 0.24f}, bottom{0.125f, 0.13f, 0.14f}; // face, top to bottom
        Color edge{0.30f, 0.315f, 0.33f}, edgeOn{0.88f, 0.90f, 0.92f};  // outline idle and on
        float radius = 4.0f;
    } button;

    // The slate look: a dark console, soft gradient and grain, dark caps and buttons, printed in white.
    static ClassicConfig slate();
    // The silver look: worn silver paint, black ink, knurled metal knobs, lever switches.
    static ClassicConfig silver();
    // The forge look: forged iron plates with copper trim and rivets, dark gunmetal knobs with value and modulation
    // arcs, amber lettering, cyan for what is live.
    static ClassicConfig forge();
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
    k.popup = rgb(18, 20, 23, 250);
    k.tooltip = rgb(14, 16, 18, 245);
    c.theme.metrics.labelPx = 8.5f;
    c.theme.metrics.valuePx = 11.0f;
    c.theme.metrics.titlePx = 9.0f;
    c.theme.metrics.popupPx = 9.0f;
    c.theme.metrics.popupRow = 18;
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
    c.display.top = {0.02f, 0.055f, 0.03f};
    c.display.bottom = {0.03f, 0.08f, 0.045f};
    c.display.edge = {0.38f, 0.39f, 0.40f};
    c.display.ink = 0xFF45E86A;
    Theme::Colors& k = c.theme.colors;
    k.background = rgb(200, 201, 198);
    k.text = c.ink;
    k.textDim = c.inkDim;
    k.textFaint = rgb(150, 152, 154);
    k.accent = rgb(200, 202, 205);
    k.focus = rgb(21, 122, 28);
    k.track = rgb(0, 0, 0, 90);
    k.hairline = rgb(0, 0, 0, 50);
    k.popup = rgb(226, 227, 224, 252);
    k.tooltip = rgb(236, 237, 234, 248);
    c.theme.metrics.labelPx = 9.5f;
    c.theme.metrics.valuePx = 11.0f;
    c.theme.metrics.titlePx = 9.5f;
    c.theme.metrics.popupPx = 9.5f;
    c.theme.metrics.popupRow = 18;
    return c;
}

inline ClassicConfig ClassicConfig::forge() {
    ClassicConfig c;
    c.ink = 0xFFD89A40;
    c.inkDim = 0xFF8A6A3A;
    c.frameInk = 0xFF8C5224;
    c.valueInk = 0xFFFFCC00;
    c.inkOn = 0xFFFFCC00;
    c.wear = 0.0f;
    c.surfaceLight = {0.082f, 0.086f, 0.094f};
    c.surfaceDark = {0.066f, 0.070f, 0.078f};
    c.knob.shape = KnobShape::Knurled;
    c.knob.ticks = false;
    c.knob.topRow = 15.0f;
    c.knob.bottomRow = 0.0f;
    c.knob.labelBelow = false;
    c.knob.showValue = false;
    c.knob.startDeg = 135.0f;
    c.knob.sweepDeg = 270.0f;
    c.knob.arc.on = true;
    c.knob.metalFoot = {0.07f, 0.075f, 0.08f};
    c.knob.metalGrip = {0.19f, 0.205f, 0.22f};
    c.knob.metalChamfer = {0.25f, 0.27f, 0.29f};
    c.knob.metalTop = {0.15f, 0.16f, 0.175f};
    c.knob.pointer = 0xFFFF3300;
    c.knob.bezel = 0xFF8C5224;
    c.toggle = ToggleShape::Lever;
    c.lever.plate = {0.16f, 0.17f, 0.18f};
    c.lever.handle = {0.62f, 0.40f, 0.22f};
    c.lever.lamp = true;
    c.frame.plate = true;
    c.display.top = {0.055f, 0.063f, 0.07f};
    c.display.bottom = {0.07f, 0.078f, 0.086f};
    c.display.edge = {0.55f, 0.32f, 0.14f};
    c.display.ink = 0xFFFFCC00;
    c.button.top = {0.165f, 0.176f, 0.188f};
    c.button.bottom = {0.10f, 0.106f, 0.114f};
    c.button.edge = {0.55f, 0.32f, 0.14f};
    c.button.edgeOn = {1.0f, 0.54f, 0.0f};
    c.button.radius = 2.0f;
    Theme::Colors& k = c.theme.colors;
    k.background = rgb(20, 21, 23);
    k.text = c.ink;
    k.textDim = c.inkDim;
    k.textFaint = c.frameInk;
    k.accent = rgb(184, 188, 192);
    k.focus = rgb(0, 229, 255);
    k.popup = rgb(14, 16, 18, 250);
    k.tooltip = rgb(10, 12, 14, 245);
    k.hairline = rgb(140, 82, 36, 120);
    c.theme.metrics.labelPx = 8.5f;
    c.theme.metrics.valuePx = 11.0f;
    c.theme.metrics.titlePx = 9.0f;
    c.theme.metrics.popupPx = 9.0f;
    c.theme.metrics.popupRow = 18;
    return c;
}

} // namespace spux::inline v1::ui::classic
