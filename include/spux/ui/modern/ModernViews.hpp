// Generic live views in the Modern style: a spectrum skyline, scrolling traces, a stereo level meter and
// an editable response curve. Each draws inside the rectangle it is given, reads only the data it is
// handed (the telemetry consumers of Live.hpp feed them) and stays correct with the CPU caches: it is a
// Gui view (GuiCore::view), redrawn when its data changed. Pass an id that is unique within the Gui's id
// scope. Looks follow the style's ModernConfig (glow, accents are the caller's colors).
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "spux/ui/Gui.hpp"
#include "spux/ui/modern/ModernStyle.hpp"

namespace spux::inline v1::ui::modern {

// A log-spaced skyline: `bands` values 0..1 (Spectrum::bands()), filled under a glowing outline.
void drawSpectrum(const ModernStyle& style, GuiCore& g, const char* id, const Rect& area, const float* bands, int count,
                  Color accent, float opacity = 1.0f);

// One row of drawTraces.
struct TraceRow {
    const char* label = "";
    Color color = 0;
    const float* values = nullptr; // oldest first (History::copyTo)
    int count = 0;
    bool bipolar = false; // the range is -1..1 with a center line, else 0..1
    float current = 0.0f; // shown as a signed number at the right end of the row
};

// Scrolling histories, one row each: label, plot, current value. Rows share the area's height.
void drawTraces(const ModernStyle& style, GuiCore& g, const char* id, const Rect& area, const TraceRow* rows,
                int rowCount);

// Where a meter's colors change, in dB.
struct MeterScale {
    float floorDb = -54.0f; // the empty end
    float warnDb = -10.8f;  // amber from here
    float clipDb = -4.3f;   // red from here
    Color ok = rgb(110, 226, 160), warn = rgb(255, 196, 80), clip = rgb(255, 96, 80);
};

// Two horizontal level bars (left over right) for linear peak levels (LevelMeter::level()).
void drawStereoMeter(const ModernStyle& style, GuiCore& g, const char* id, const Rect& area, float levelLeft,
                     float levelRight, const MeterScale& scale = {});

// What the curve editor keeps between frames.
struct CurveEditorState {
    int dragging = -1;
    float startValue = 0.0f;
};

// An editable response curve: `count` points 0..1 at equal spacing across the plot, draggable vertically
// (one gesture of `gestureParam` per drag when it is a parameter). `curve` evaluates the line that is drawn
// (the plugin's own interpolation); without it the line joins the points. Returns true when a point moved.
bool drawCurveEditor(const ModernStyle& style, GuiCore& g, const char* id, const Rect& area, const char* title,
                     float* points, int count, const std::function<float(float)>& curve, Color accent,
                     CurveEditorState& state, std::uint32_t gestureParam = kNoParamId);

} // namespace spux::inline v1::ui::modern
