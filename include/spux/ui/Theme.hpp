// A flavor's look as data (decision U8): colours, metrics and font slots, plus the interaction feel the
// widgets share. A flavor provides its default Theme; a plugin can supply its own values. Painters read
// the colours and metrics; Gui reads the interaction metrics. Nothing here is code.
#pragma once

#include "spux/ui/Geometry.hpp"

namespace spux::inline v1::ui {

struct Theme {
    struct Colors {
        Color background = rgb(6, 8, 18);
        Color text = rgb(234, 238, 248);
        Color textDim = rgb(152, 162, 188);
        Color textFaint = rgb(98, 108, 134);
        Color panel = rgb(12, 16, 30, 148);
        Color raised = rgb(255, 255, 255, 14);
        Color hairline = rgb(255, 255, 255, 26);
        Color track = rgb(255, 255, 255, 26);
        Color accent = rgb(112, 170, 255);     // a widget without its own accent
        Color modulation = rgb(204, 144, 255); // modulation rings and depth handles
        Color focus = rgb(112, 170, 255);
        Color warn = rgb(255, 150, 70);
        Color popup = rgb(10, 14, 28, 242);
        Color tooltip = rgb(8, 11, 22, 240);
    } colors;

    struct Metrics {
        float labelPx = 10.5f; // widget labels
        float valuePx = 11.5f; // values
        float titlePx = 11.0f; // panel titles
        float popupPx = 12.0f;
        int popupRow = 26; // popup row height
        int minKnob = 40;  // smallest knob bounds that still reads
        int focusRing = 3; // gap between a widget and its focus ring
    } metrics;

    // Interaction feel: the same for every flavor unless a plugin changes it.
    struct Feel {
        double doubleClick = 0.35; // seconds between the presses of a double click
        double tooltipDelay = 0.6; // seconds of hovering before a tooltip shows
        float dragPixels = 166.0f; // vertical drag for the full range of a knob or slider
        float fine = 0.1f;         // shift: drag and wheel steps times this
        double wheelStep = 0.02;   // normalized step of one wheel notch (stepped kinds: one step)
        double keyStep = 0.02;     // normalized step of an arrow key
    } feel;

    // Font slots a renderer maps to its fonts (Retro bitmap sizes, Classic atlases, Modern SDF weights).
    struct Fonts {
        int label = 0, value = 0, title = 0;
    } fonts;
};

} // namespace spux::inline v1::ui
