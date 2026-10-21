// The Retro flavor's look as data (guidelines 7, "Painters and themes"): the bitmap fonts, the ink, the surface,
// the colors and shapes of the controls. Three looks ship, each defined in an object of its own so that a program
// links only the fonts of the look it uses:
//
//   silver()   a silver panel, fluted knobs with a printed scale, lever switches, dark readouts (5x7 and 7-pixel
//   faces); tracker()  a flat grey tracker screen: bevelled keys, blue selections, an 8x8 face (the pt2 font); forge()
//   forged iron on a dark ground, copper outlines and amber lettering around the same silver knobs.
//
// A plugin starts from one and changes what it wants; every color is an opaque or translucent ARGB value, and a
// color of 0 where it says so means "the default" (usually the ink).
#pragma once

#include "spux/ui/Theme.hpp"
#include "spux/ui/retro/BitmapFont.hpp"

namespace spux::inline v1::ui::retro {

// A fluted metal knob with a conical top, or a flat bevelled disc.
enum class KnobShape { Fluted, Flat };
// A lever in a slot, or a bevelled key with a lamp.
enum class ToggleShape { Lever, Key };

struct RetroConfig {
    Theme theme;

    // Labels, values and lists; and the panel titles (null: the same font). The fonts must outlive the style.
    const BitmapFont* font = nullptr;
    const BitmapFont* titleFont = nullptr;

    // Names, scales and units, a selected step, the value of a control, and the lines of a frame.
    Color ink = 0xFF16181A, inkDim = 0xFF5C6064, inkOn = 0xFF157A1C, valueInk = 0xFF16181A, frameInk = 0xFF808488;

    // What a panel without a title is: the flat surface, and optionally the trims along its top and bottom edges
    // (an outlined band, a shadow line and a light line).
    Color surface = 0xFFDBDFE1;
    struct Trims {
        bool on = false;
        Color line = 0xFFC0C4C8, shadow = 0xFF808488, light = 0xFFFFFFFF;
        int top = 12, bottom = 13; // band heights
    } trims;

    // What a titled panel is: an outline with the title cut into its top line (etched: a dark line with a light one
    // beside it), or a forged plate (a gradient, a trim line inside the edge, rivets in the corners, the title on a
    // plate on the top edge).
    struct Frame {
        bool etched = false;
        bool plate = false;
        Color border = 0xFF16181A, fillTop = 0xFF202326, fillBottom = 0xFF121416;
        Color trim = 0xFF8C5224, trimInner = 0xFF1A1C1E, titleFill = 0xFF101214;
        Color titleInk = 0; // 0: the ink
        bool rivets = true;
    } frame;

    struct Knob {
        KnobShape shape = KnobShape::Fluted;
        bool ticks = true;       // the printed scale of 11 ticks around the dial
        bool indexSquare = true; // the square above the 12 o'clock tick
        bool shadow = true;      // a soft drop shadow down and to the right
        // Rows above and below the dial for the label and the value; 0 when the page prints its own.
        int topRow = 9, bottomRow = 9;
        bool labelBelow = false; // the label goes in the bottom row and the value in the top row; else the reverse
        bool showValue = true;
        float startDeg = 120.0f, sweepDeg = 300.0f; // the dial: 7 o'clock, clockwise (degrees, 0 = 3 o'clock)
        int pointerWidth = 2;
        Color tick = 0; // 0: the ink
        Color bezel = 0xFF26292C, skirt = 0xFF7C8288, rim = 0xFFAEB4BA, flute = 0xFF686E74;
        Color face = 0xFFB9BFC4, sheen = 0xFFD2D7DB, faceEdge = 0xFF888E94;
        Color pointer = 0;       // 0: the ink
        Color pick = 0xFF00E5FF; // the ring of a knob that can be picked right now
    } knob;

    ToggleShape toggle = ToggleShape::Lever;
    // The lever switch: the frame, the metal plate, the well, the handle (edge, face, inner square), and a lamp beside
    // the plate that is lit when the switch is on.
    struct Lever {
        Color edge = 0xFF26292C, metal = 0xFF8A9096, well = 0xFF1A1C1E;
        Color handleEdge = 0xFF2E3134, handleFace = 0xFFE2E6EA, handleInner = 0xFFB4BAC0;
        bool lamp = false;
    } lever;

    // A fader's slot, the part of it that shows the value (an accent of the widget replaces it), and the cap:
    // `capLong` pixels along the slot, `capShort` across.
    struct Slider {
        Color slot = 0xFF1A1C1E, slotEdge = 0xFF8A9096, fill = 0xFF157A1C;
        int capLong = 7, capShort = 15;
        int topRow = 9, bottomRow = 9; // value above, label below (vertical faders)
    } slider;

    // The bevelled face of buttons, tabs, the steps of a selector and the parts of a stepper: idle and on, the outline,
    // the light and dark edges of the bevel, and the ink on the face (and when on).
    struct Button {
        Color face = 0xFFC0C4C8, faceOn = 0xFF808488, edge = 0xFF202020;
        Color light = 0xFFFFFFFF, dark = 0xFF808488;
        Color ink = 0xFF16181A, inkOn = 0xFFFFFFFF;
        bool outline = true; // the outline around the face; off: the bevel alone
    } button;

    // The recessed display of a dropdown, a stepper's value, a text field and the readout: dark glass with an edge.
    struct Display {
        Color fill = 0xFF101010, edge = 0xFF808488, ink = 0xFFE0E4E8, inkDim = 0xFF9AA0A6;
        bool sunken = false; // draw the edge as a sunken bevel (light below and right) instead of a line
    } display;

    // The lamp: its rim.
    Color ledEdge = 0xFF202020;

    struct Popup {
        Color fill = 0xFF181A1C, edge = 0xFF808488, ink = 0xFFE0E4E8, inkDim = 0xFF8A9096, hot = 0xFF157A1C,
              hotInk = 0xFFFFFFFF;
    } popup;
    struct Tooltip {
        Color fill = 0xFFF4F0C8, edge = 0xFF202020, ink = 0xFF101010, inkDim = 0xFF505040;
    } tooltip;

    // Keys: the felt strip behind them, white and black keys idle and held, the line between white keys.
    struct Keyboard {
        Color felt = 0xFF5A1E1A, white = 0xFFEDEBE3, whiteDown = 0xFFC8C4B0, black = 0xFF111111, blackDown = 0xFF3A3020,
              edge = 0xFF8A8A80;
    } keyboard;

    static RetroConfig silver();
    static RetroConfig tracker();
    static RetroConfig forge();
};

} // namespace spux::inline v1::ui::retro
