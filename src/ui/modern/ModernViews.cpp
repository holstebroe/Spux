#include "spux/ui/modern/ModernViews.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "ModernInternal.hpp"

namespace spux::inline v1::ui::modern {

namespace {

// FNV-1a over what a view shows: the view is redrawn when this changes.
class Key {
public:
    Key& add(const void* p, std::size_t n) {
        const auto* b = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i) {
            h_ ^= b[i];
            h_ *= 1099511628211ull;
        }
        return *this;
    }
    template <class T> Key& operator<<(const T& v) { return add(&v, sizeof(T)); }
    Key& values(const float* v, int n) {
        return n > 0 && v ? add(v, sizeof(float) * static_cast<std::size_t>(n)) : *this;
    }
    Key& text(const char* s) { return s ? add(s, std::strlen(s)) : *this; }
    [[nodiscard]] std::uint64_t value() const { return h_; }

private:
    std::uint64_t h_ = 1469598103934665603ull;
};

float clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }

} // namespace

// --- spectrum --------------------------------------------------------------------------------------------

void drawSpectrum(const ModernStyle& style, GuiCore& g, const char* id, const Rect& area, const float* bands, int count,
                  Color accent, float opacity) {
    if (area.empty() || count < 2 || !bands)
        return;
    Key k;
    k << area.x << area.y << area.w << area.h << accent << opacity;
    k.values(bands, count);
    if (!g.view(g.id(id), area, k.value()))
        return;
    IRenderer& r = g.r();
    const ModernConfig& c = style.config();
    std::vector<Point> pts(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
        pts[static_cast<std::size_t>(i)] = {f(area.x) + f(area.w) * (f(i) + 0.5f) / f(count),
                                            f(area.bottom()) - clamp01(bands[i]) * f(area.h)};
    r.pushClip(area);
    lit(r, c, 1.0f);
    r.fillUnder(pts.data(), count, f(area.bottom()), halo(c, withAlpha(accent, 0.30f * opacity), 1.0f),
                withAlpha(accent, 0.0f));
    lit(r, c, 1.3f);
    r.drawPolyline(pts.data(), count, withAlpha(accent, 0.75f * opacity), 1.6f);
    unlit(r);
    r.popClip();
}

// --- traces ----------------------------------------------------------------------------------------------

void drawTraces(const ModernStyle& style, GuiCore& g, const char* id, const Rect& area, const TraceRow* rows,
                int rowCount) {
    if (area.empty() || rowCount <= 0 || !rows)
        return;
    Key k;
    k << area.x << area.y << area.w << area.h;
    for (int i = 0; i < rowCount; ++i) {
        k.text(rows[i].label) << rows[i].color << rows[i].bipolar << rows[i].current << rows[i].count;
        k.values(rows[i].values, rows[i].count);
    }
    if (!g.view(g.id(id), area, k.value()))
        return;
    IRenderer& r = g.r();
    const Theme& t = style.theme;
    (void)style.config();
    const float px = t.metrics.labelPx;
    const int rowH = area.h / rowCount;
    const int labelW = 78, valueW = 44;
    r.pushClip(area);
    for (int i = 0; i < rowCount; ++i) {
        const TraceRow& row = rows[i];
        const Rect strip{area.x, area.y + i * rowH, area.w, rowH};
        const Rect plot{strip.x + labelW, strip.y + 3, strip.w - labelW - valueW, strip.h - 6};
        r.drawTextPx(row.label, f(strip.x), capY(f(strip.y), f(strip.h), px), t.colors.textDim, px, Align::Left,
                     FontWeight::Bold);
        r.fillRoundRect(plot, 4.0f, withAlpha(kWhite, 0.035f));
        if (row.count > 2 && row.values && plot.w > 4) {
            const int n = std::min(row.count, plot.w / 2);
            const float* v = row.values + (row.count - n);
            r.pushClip(plot);
            if (row.bipolar)
                r.drawLine({f(plot.x), f(plot.y) + f(plot.h) * 0.5f}, {f(plot.right()), f(plot.y) + f(plot.h) * 0.5f},
                           hairline(0.08f), 1.0f);
            r.drawTrace(v, n, plot, row.bipolar ? -1.0f : 0.0f, 1.0f, row.color);
            r.popClip();
        }
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%+.2f", static_cast<double>(row.current));
        r.drawTextPx(buf, f(strip.right()), capY(f(strip.y), f(strip.h), px), t.colors.textDim, px, Align::Right);
    }
    r.popClip();
}

// --- stereo meter ----------------------------------------------------------------------------------------

void drawStereoMeter(const ModernStyle& style, GuiCore& g, const char* id, const Rect& area, float levelLeft,
                     float levelRight, const MeterScale& scale) {
    if (area.empty())
        return;
    // the bars change in steps of a pixel, so that is the resolution that matters for redrawing
    auto position = [&](float level) {
        const float db = 20.0f * std::log10(level + 1e-6f);
        return clamp01((db - scale.floorDb) / -scale.floorDb);
    };
    const float tl = position(levelLeft), tr = position(levelRight);
    Key k;
    k << area.x << area.y << area.w << area.h << static_cast<int>(tl * f(area.w)) << static_cast<int>(tr * f(area.w))
      << scale.ok << scale.warn << scale.clip;
    if (!g.view(g.id(id), area, k.value()))
        return;
    IRenderer& r = g.r();
    const ModernConfig& c = style.config();
    auto bar = [&](int row, float t) {
        const Rect b{area.x, area.y + row * (area.h / 2), area.w, area.h / 2 - 2};
        const float rad = f(b.h) * 0.5f;
        r.fillRoundRect(b, rad, hairline(0.07f));
        if (t <= 0.01f)
            return;
        const float db = scale.floorDb - scale.floorDb * t;
        const Color col = db > scale.clipDb ? scale.clip : db > scale.warnDb ? scale.warn : scale.ok;
        lit(r, c, 1.4f);
        r.fillRoundRect({b.x, b.y, std::max(2, static_cast<int>(f(b.w) * t)), b.h}, rad, col);
        unlit(r);
    };
    r.pushClip(area);
    bar(0, tl);
    bar(1, tr);
    r.popClip();
}

// --- curve editor ----------------------------------------------------------------------------------------

bool drawCurveEditor(const ModernStyle& style, GuiCore& g, const char* id, const Rect& area, const char* title,
                     float* points, int count, const std::function<float(float)>& curve, Color accent,
                     CurveEditorState& state, std::uint32_t gestureParam) {
    if (area.empty() || count < 2 || !points)
        return false;
    const Theme& t = style.theme;
    const ModernConfig& c = style.config();
    const Rect plot{area.x + 14, area.y + 34, area.w - 28, area.h - 48};
    if (plot.empty())
        return false;
    // input first: the handles are interactions whatever gets drawn
    bool changed = false;
    bool anyHot = false;
    int hotIndex = -1;
    g.pushId(id);
    for (int i = 0; i < count; ++i) {
        const float x = f(plot.x) + f(plot.w) * f(i) / f(count - 1);
        const Point p{x, f(plot.bottom()) - clamp01(points[i]) * f(plot.h)};
        g.pushId(static_cast<std::uint64_t>(i));
        const Interaction h = g.handle("point", p, 9.0f, gestureParam);
        g.popId();
        if (h.pressed) {
            state.dragging = i;
            state.startValue = points[i];
        }
        if (h.active && state.dragging == i) {
            const float v = clamp01(state.startValue - h.dy / f(plot.h));
            if (v != points[i]) {
                points[i] = v;
                changed = true;
            }
        }
        if (h.hot || h.active) {
            anyHot = true;
            hotIndex = i;
        }
    }
    g.popId();
    if (!g.leftDown())
        state.dragging = -1;

    Key k;
    k << area.x << area.y << area.w << area.h << accent << hotIndex << anyHot;
    k.text(title).values(points, count);
    if (!g.view(g.id(id), area, k.value()))
        return changed;
    IRenderer& r = g.r();
    r.pushClip(area);
    if (title && *title)
        r.drawTextPx(title, f(area.x) + 14.0f, f(area.y) + 12.0f, t.colors.textDim, t.metrics.labelPx, Align::Left,
                     FontWeight::Bold);
    r.fillRoundRect(plot, 6.0f, hairline(0.04f));
    r.drawLine({f(plot.x), f(plot.bottom())}, {f(plot.right()), f(plot.y)}, hairline(0.1f), 1.0f);
    std::vector<Point> line;
    constexpr int kSteps = 48;
    for (int i = 0; i <= kSteps; ++i) {
        const float x = f(i) / f(kSteps);
        float y;
        if (curve) {
            y = curve(x);
        } else {
            const float pos = x * f(count - 1);
            const int i0 = std::min(static_cast<int>(pos), count - 2);
            y = points[i0] + (points[i0 + 1] - points[i0]) * (pos - f(i0));
        }
        line.push_back({f(plot.x) + x * f(plot.w), f(plot.bottom()) - clamp01(y) * f(plot.h)});
    }
    lit(r, c, 1.0f);
    r.fillUnder(line.data(), static_cast<int>(line.size()), f(plot.bottom()), halo(c, withAlpha(accent, 0.22f), 1.0f),
                withAlpha(accent, 0.0f));
    lit(r, c, 1.6f);
    r.drawPolyline(line.data(), static_cast<int>(line.size()), accent, 2.0f);
    unlit(r);
    for (int i = 0; i < count; ++i) {
        const Point p{f(plot.x) + f(plot.w) * f(i) / f(count - 1), f(plot.bottom()) - clamp01(points[i]) * f(plot.h)};
        const bool hot = i == hotIndex;
        lit(r, c, hot ? 2.2f : 1.3f);
        r.fillCircle(p, hot ? 5.5f : 4.0f, accent);
        unlit(r);
    }
    r.popClip();
    return changed;
}

} // namespace spux::inline v1::ui::modern
