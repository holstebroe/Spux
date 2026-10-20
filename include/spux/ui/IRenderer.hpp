// The renderer interface every flavor implements (guidelines 7, "Renderer contract").
// Two parts: the basics every renderer implements, and the effect extensions with
// plain default implementations, so any renderer can stand in for any other (Liskov). Plugin code never
// asks which renderer it runs on. Coordinates are logical pixels, colors straight-alpha ARGB.
//
// A CPU renderer that keeps its pixels between frames offers layers() (Layers.hpp): the background
// cache and the per-widget skip with a dirty region. A renderer that redraws everything every frame
// (the GPU) returns null and Gui draws every widget every frame.
#pragma once

#include <cmath>

#include "spux/ui/Geometry.hpp"

namespace spux::inline v1::ui {

class LayerCache;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    [[nodiscard]] virtual int width() const = 0;
    [[nodiscard]] virtual int height() const = 0;

    // --- basics -------------------------------------------------------------------------------------------
    virtual void clear(Color c) = 0;

    // Everything drawn after pushClip is limited to the intersection of the clip rectangles.
    virtual void pushClip(const Rect& r) = 0;
    virtual void popClip() = 0;

    virtual void fillRect(const Rect& r, Color c) = 0;
    virtual void strokeRect(const Rect& r, Color c, float thickness = 1.0f) = 0;
    virtual void fillRoundRect(const Rect& r, float radius, Color c) = 0;
    virtual void fillGradientRect(const Rect& r, Color top, Color bottom) = 0;
    virtual void strokeRoundRect(const Rect& r, float /*radius*/, Color c, float thickness = 1.0f) {
        strokeRect(r, c, thickness);
    }

    virtual void drawLine(Point a, Point b, Color c, float thickness = 1.0f) = 0;
    virtual void drawPolyline(const Point* pts, int count, Color c, float thickness = 1.0f) = 0;
    virtual void fillCircle(Point center, float radius, Color c) = 0;
    virtual void strokeCircle(Point center, float radius, Color c, float thickness = 1.0f) = 0;

    // Text in the renderer's label font; scale multiplies the glyph size.
    virtual void drawText(const char* text, int x, int y, Color c, int scale = 1) = 0;
    [[nodiscard]] virtual int textWidth(const char* text, int scale = 1) const = 0;
    [[nodiscard]] virtual int textHeight(int scale = 1) const = 0;

    // --- caches (CPU renderers) ----------------------------------------------------------------------------
    // The layers of a renderer that keeps its pixels between frames; null: redraw everything per frame.
    [[nodiscard]] virtual LayerCache* layers() { return nullptr; }

    // --- effect extensions, plain by default ---------------------------------------------------------------
    [[nodiscard]] virtual bool isGpu() const { return false; }
    // The animated backdrop; plain: a flat gradient. `time` in seconds.
    virtual void drawBackground(Color a, Color b, float /*time*/) { fillGradientRect({0, 0, width(), height()}, a, b); }
    // Blend mode and emission for what is drawn next. Emission above 1 makes colors glow through the bloom.
    virtual void setBlend(Blend) {}
    virtual void setEmission(float) {}
    // Arc of a ring between angles a0 < a1 (radians, 0 = 3 o'clock, clockwise on screen), round caps.
    virtual void drawArc(Point center, float radius, float a0, float a1, Color c, float thickness) {
        constexpr int kSteps = 48;
        Point pts[kSteps + 1];
        for (int i = 0; i <= kSteps; ++i) {
            const float a = a0 + (a1 - a0) * static_cast<float>(i) / kSteps;
            pts[i] = {center.x + std::cos(a) * radius, center.y + std::sin(a) * radius};
        }
        drawPolyline(pts, kSteps + 1, c, thickness);
    }
    // A soft round blob: `c` at the center fading to nothing at `radius`.
    virtual void drawGlow(Point center, float radius, Color c) {
        fillCircle(center, radius * 0.5f, withAlpha(c, 0.25f));
    }
    virtual void fillRadialGradient(Point center, float radius, Color inner, Color /*outer*/) {
        fillCircle(center, radius, inner);
    }
    // A soft shadow under a rounded rectangle; plain: none.
    virtual void drawShadow(const Rect&, float /*radius*/, float /*spread*/, Color) {}
    // A glass panel: translucent body, a light rim and a top highlight; plain: a filled round rect.
    virtual void drawPanel(const Rect& r, float radius, Color tint) {
        fillRoundRect(r, radius, tint);
        strokeRect(r, withAlpha(0xFFFFFFFFu, 0.12f), 1.0f);
    }
    // Polyline with a color and a width per point (fading trails, traces); plain: segments.
    virtual void drawStrip(const Point* pts, const Color* colors, const float* widths, int count) {
        for (int i = 0; i + 1 < count; ++i)
            drawLine(pts[i], pts[i + 1], colors[i], widths[i]);
    }
    // A band between a polyline and a baseline, fading from `top` at the line to `bottom` at the baseline.
    virtual void fillUnder(const Point* pts, int count, float baselineY, Color top, Color bottom) {
        for (int i = 0; i + 1 < count; ++i) {
            const int x0 = static_cast<int>(pts[i].x), x1 = static_cast<int>(pts[i + 1].x);
            const int yTop = static_cast<int>(pts[i].y < pts[i + 1].y ? pts[i].y : pts[i + 1].y);
            if (x1 > x0 && baselineY > static_cast<float>(yTop))
                fillGradientRect({x0, yTop, x1 - x0, static_cast<int>(baselineY) - yTop}, top, bottom);
        }
    }
    // Text at a pixel size (a GPU font scales continuously, a bitmap font in whole steps).
    virtual void drawTextPx(const char* text, float x, float y, Color c, float px, Align align = Align::Left,
                            FontWeight = FontWeight::Regular) {
        const int sc = bitmapScale(px);
        const int w = textWidth(text, sc);
        const float left = align == Align::Left     ? x
                           : align == Align::Center ? x - static_cast<float>(w) * 0.5f
                                                    : x - static_cast<float>(w);
        drawText(text, static_cast<int>(left), static_cast<int>(y), c, sc);
    }
    [[nodiscard]] virtual float textWidthPx(const char* text, float px, FontWeight = FontWeight::Regular) const {
        return static_cast<float>(textWidth(text, bitmapScale(px)));
    }
    // A trace of n values between lo and hi, spread over the width of the area; plain: a polyline.
    virtual void drawTrace(const float* values, int n, const Rect& area, float lo, float hi, Color c) {
        constexpr int kMax = 512;
        Point pts[kMax];
        const int m = n < kMax ? n : kMax;
        if (m < 2 || !(hi > lo))
            return;
        for (int i = 0; i < m; ++i) {
            const float t = (values[i * n / m] - lo) / (hi - lo);
            const float tc = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
            pts[i] = {static_cast<float>(area.x) +
                          static_cast<float>(area.w - 1) * static_cast<float>(i) / static_cast<float>(m - 1),
                      static_cast<float>(area.bottom() - 1) - tc * static_cast<float>(area.h - 1)};
        }
        drawPolyline(pts, m, c, 1.0f);
    }
    // A level bar, 0..1, filling from the bottom; plain: a filled rectangle.
    virtual void drawMeter(const Rect& area, float level, Color c) {
        const float l = level < 0.0f ? 0.0f : level > 1.0f ? 1.0f : level;
        const int h = static_cast<int>(std::lround(static_cast<float>(area.h) * l));
        fillRect({area.x, area.bottom() - h, area.w, h}, c);
    }

protected:
    static int bitmapScale(float px) { return px < 15.0f ? 1 : px < 24.0f ? 2 : 3; }
};

} // namespace spux::inline v1::ui
