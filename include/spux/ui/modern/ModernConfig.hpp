// The Modern style as data. A plugin passes a ModernConfig to ModernStyle to change how every standard
// widget looks without writing a painter: colors and text sizes (the core Theme), shapes, how much light
// the widgets emit, and the proportions of the individual controls. The defaults are the style's own look:
// a dark field, glass panels, glowing accents.
//
// Sizes are logical pixels, angles radians (0 = 3 o'clock, clockwise on screen). A config can change at
// run time (ModernStyle::configure); the Gui redraws every widget afterwards (GuiCore::invalidate).
#pragma once

#include "spux/ui/Theme.hpp"

namespace spux::inline v1::ui::modern {

struct ModernConfig {
    // Colors, text sizes, interaction feel: what every flavor shares (Theme.hpp).
    Theme theme;

    struct Shape {
        float panelRadius = 16.0f;
        float fieldRadius = 8.0f; // dropdowns and text fields
        float popupRadius = 12.0f;
        float tooltipRadius = 8.0f;
        float itemRadius = 7.0f;     // the highlight of a popup row
        float stroke = 1.0f;         // hairlines around controls
        float focusStroke = 1.5f;    // the keyboard focus ring
        bool panelShadow = true;     // a shadow under panels, popups and tooltips
        float controlShadow = 10.0f; // blur of the halo under a lit chip, switch or segment; 0: none
    } shape;

    // How much light the widgets give off. With glow off, accents are flat alpha fills.
    struct Effects {
        bool glow = true;
        float emission = 1.0f;     // scales every emission level (above 1 makes the bloom stronger)
        float glowStrength = 1.0f; // scales the alpha of halos
    } effects;

    struct Knob {
        float startAngle = 2.35619449f; // 135 degrees
        float sweep = 4.71238898f;      // 270 degrees
        float trackWidth = 4.0f;
        float largeTrackWidth = 5.0f;
        float haloWidth = 9.0f; // the soft band under the value arc
        float largeHaloWidth = 11.0f;
        float largeLabelPx = 12.0f; // a large knob has larger text and heavier strokes, the same dial
        float largeValuePx = 14.0f;
        float bodyRadius = 0.72f;   // the cap, as a fraction of the dial radius
        float pointerInner = 0.28f; // the pointer line runs between these fractions of the dial radius
        float pointerOuter = 0.62f;
        float pointerWidth = 2.2f;
        float modRingGap = 6.0f; // the modulation ring sits this far outside the dial
        float handleRadius = 6.0f;
        Color bodyLight = rgb(58, 66, 92);
        Color bodyDark = rgb(16, 20, 34);
        Color pointer = 0; // 0: the theme's text color
    } knob;

    struct Toggle {
        float width = 30.0f;
        float height = 16.0f;
        float labelGap = 8.0f;
        Color thumbOn = rgb(255, 255, 255);
        Color thumbOff = rgb(170, 180, 204);
    } toggle;

    struct Slider {
        float trackHeight = 4.0f;
        float thumbRadius = 6.0f;
        float labelWidth = 72.0f; // horizontal: the label's column left of the track; 0: no label column
        float valueWidth = 62.0f; // horizontal: the value's column right of the track
        Color thumb = rgb(236, 240, 250);
    } slider;

    struct Popup {
        float padding = 6.0f;
        float itemPadding = 34.0f; // room beside the text for the mark and the margins
        float separatorHeight = 9.0f;
    } popup;

    // The standard dark theme (the default).
    static ModernConfig standard() { return {}; }
    // Raised text and panel contrast.
    static ModernConfig highContrast() {
        ModernConfig c;
        Theme& t = c.theme;
        t.colors.text = rgb(255, 255, 255);
        t.colors.textDim = rgb(205, 212, 230);
        t.colors.textFaint = rgb(150, 160, 184);
        t.colors.panel = rgb(4, 6, 14, 215);
        t.colors.hairline = rgb(255, 255, 255, 70);
        t.colors.track = rgb(255, 255, 255, 56);
        return c;
    }
};

} // namespace spux::inline v1::ui::modern
