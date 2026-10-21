#include "spux/ui/retro/PixelRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace spux::inline v1::ui::retro {

namespace {

constexpr double kEps = 1e-6;

int roundPx(float v) { return static_cast<int>(std::lround(v)); }
int brushOf(float thickness) { return std::max(1, roundPx(thickness)); }

// How many pixels the corner of a rounded rectangle takes off the row `d` rows from its top or bottom edge:
// the row's first pixel whose center is inside the corner circle.
int cornerInset(int radius, int d) {
    if (d >= radius)
        return 0;
    const double dy = static_cast<double>(radius) - (static_cast<double>(d) + 0.5);
    const double half = std::sqrt(std::max(0.0, static_cast<double>(radius) * radius - dy * dy));
    return std::max(0, static_cast<int>(std::ceil(static_cast<double>(radius) - 0.5 - half - kEps)));
}

// The pixels of row `row` whose centers are within d^2 <= limit2 of (cx, cy): [x0, x1] inclusive, or x1 < x0 when there
// are none. The limit is the squared radius, plus a little for a circle that includes its edge, minus a little for one
// that does not.
void circleSpan(double cx, double cy, double limit2, int row, int& x0, int& x1) {
    const double dy = static_cast<double>(row) + 0.5 - cy;
    const double rem = limit2 - dy * dy;
    if (rem < 0.0) {
        x0 = 1;
        x1 = 0;
        return;
    }
    const double half = std::sqrt(rem);
    x0 = static_cast<int>(std::ceil(cx - half - 0.5 - kEps));
    x1 = static_cast<int>(std::floor(cx + half - 0.5 + kEps));
}

// The edge of a circle belongs to it; pixel centers are on a lattice a quarter apart in d^2, so these margins
// decide only what rounding noise would.
double outerLimit(double r) { return r * r + 1e-6; }
double innerLimit(double r) { return r * r - 1e-4; }

} // namespace

PixelRenderer::PixelRenderer(int width, int height)
    : layers_(std::max(1, width), std::max(1, height), 1), font_(&font5x7()), bold_(font_) {
    clips_.reserve(16);
}

PixelRenderer::PixelRenderer(int width, int height, const BitmapFont& font, const BitmapFont* bold)
    : layers_(std::max(1, width), std::max(1, height), 1), font_(&font), bold_(bold ? bold : &font) {
    clips_.reserve(16);
}

std::vector<std::uint32_t> PixelRenderer::frame() const {
    std::vector<std::uint32_t> out(static_cast<std::size_t>(width()) * static_cast<std::size_t>(height()));
    layers_.resolve(out.data(), {0, 0, width(), height()});
    return out;
}

void PixelRenderer::pushClip(const Rect& r) { clips_.push_back(clip().intersect(r)); }

void PixelRenderer::popClip() {
    if (!clips_.empty())
        clips_.pop_back();
}

void PixelRenderer::clear(Color c) {
    std::uint32_t* p = layers_.target();
    std::fill(p, p + static_cast<std::size_t>(width()) * static_cast<std::size_t>(height()), c | 0xFF000000u);
}

// --- pixels --------------------------------------------------------------------------------------------------

void PixelRenderer::blendAt(std::uint32_t* p, Color c) const {
    const std::uint32_t a = c >> 24;
    if (a == 0)
        return;
    if (blend_ == Blend::Alpha && a == 255) {
        *p = c | 0xFF000000u;
        return;
    }
    const std::uint32_t d = *p;
    std::uint32_t out = 0xFF000000u;
    for (int sh = 0; sh <= 16; sh += 8) {
        const std::uint32_t s = (c >> sh) & 255u, t = (d >> sh) & 255u;
        const std::uint32_t v =
            blend_ == Blend::Add ? std::min(255u, t + (s * a + 127u) / 255u) : (s * a + t * (255u - a) + 127u) / 255u;
        out |= v << sh;
    }
    *p = out;
}

void PixelRenderer::pixel(int x, int y, Color c) {
    if (!clip().contains(x, y))
        return;
    blendAt(layers_.target() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width()) +
                static_cast<std::size_t>(x),
            c);
}

void PixelRenderer::fillSpan(int x0, int x1, int y, Color c, const Rect& cl) {
    if (y < cl.y || y >= cl.bottom())
        return;
    x0 = std::max(x0, cl.x);
    x1 = std::min(x1, cl.right());
    if (x1 <= x0)
        return;
    std::uint32_t* row = layers_.target() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width());
    if (blend_ == Blend::Alpha && (c >> 24) == 255) {
        std::fill(row + x0, row + x1, c);
        return;
    }
    for (int x = x0; x < x1; ++x)
        blendAt(row + x, c);
}

// --- rectangles ------------------------------------------------------------------------------------------------

void PixelRenderer::fillRect(const Rect& r, Color c) {
    const Rect cl = clip(), v = r.intersect(cl);
    for (int y = v.y; y < v.bottom(); ++y)
        fillSpan(v.x, v.right(), y, c, cl);
}

void PixelRenderer::strokeRect(const Rect& r, Color c, float thickness) {
    if (r.empty())
        return;
    const int n = brushOf(thickness);
    if (2 * n >= std::min(r.w, r.h)) {
        fillRect(r, c);
        return;
    }
    fillRect({r.x, r.y, r.w, n}, c);
    fillRect({r.x, r.bottom() - n, r.w, n}, c);
    fillRect({r.x, r.y + n, n, r.h - 2 * n}, c);
    fillRect({r.right() - n, r.y + n, n, r.h - 2 * n}, c);
}

void PixelRenderer::fillRoundRect(const Rect& r, float radius, Color c) {
    if (r.empty())
        return;
    const int R = std::clamp(roundPx(radius), 0, std::min(r.w, r.h) / 2);
    const Rect cl = clip();
    for (int y = 0; y < r.h; ++y) {
        const int inset = cornerInset(R, std::min(y, r.h - 1 - y));
        fillSpan(r.x + inset, r.right() - inset, r.y + y, c, cl);
    }
}

void PixelRenderer::strokeRoundRect(const Rect& r, float radius, Color c, float thickness) {
    if (r.empty())
        return;
    const int n = brushOf(thickness);
    const int R = std::clamp(roundPx(radius), 0, std::min(r.w, r.h) / 2);
    const Rect in = r.inset(n);
    if (in.empty()) {
        fillRoundRect(r, radius, c);
        return;
    }
    const int Ri = std::clamp(R - n, 0, std::min(in.w, in.h) / 2);
    const Rect cl = clip();
    for (int y = 0; y < r.h; ++y) {
        const int o = cornerInset(R, std::min(y, r.h - 1 - y));
        const int ox0 = r.x + o, ox1 = r.right() - o;
        const int iy = y - n;
        if (iy < 0 || iy >= in.h) {
            fillSpan(ox0, ox1, r.y + y, c, cl);
            continue;
        }
        const int i = cornerInset(Ri, std::min(iy, in.h - 1 - iy));
        fillSpan(ox0, in.x + i, r.y + y, c, cl);
        fillSpan(in.right() - i, ox1, r.y + y, c, cl);
    }
}

void PixelRenderer::fillGradientRect(const Rect& r, Color top, Color bottom) {
    const Rect cl = clip();
    for (int y = 0; y < r.h; ++y)
        fillSpan(r.x, r.right(), r.y + y,
                 ui::mix(top, bottom, (static_cast<float>(y) + 0.5f) / static_cast<float>(r.h)), cl);
}

void PixelRenderer::bevel(const Rect& r, Color light, Color dark, bool sunken) {
    if (r.empty())
        return;
    if (sunken)
        std::swap(light, dark);
    fillRect({r.x, r.y, r.w - 1, 1}, light);
    fillRect({r.x, r.y, 1, r.h - 1}, light);
    fillRect({r.x, r.bottom() - 1, r.w, 1}, dark);
    fillRect({r.right() - 1, r.y, 1, r.h}, dark);
}

void PixelRenderer::fillDither(const Rect& r, Color c, int level) {
    const Rect v = r.intersect(clip());
    for (int y = v.y; y < v.bottom(); ++y)
        for (int x = v.x; x < v.right(); ++x)
            if (dither(x, y, level))
                pixel(x, y, c);
}

// --- lines and circles -------------------------------------------------------------------------------------

void PixelRenderer::stamp(int x, int y, int brush, Color c) {
    if (brush == 1) {
        pixel(x, y, c);
        return;
    }
    const int off = (brush - 1) / 2;
    fillRect({x - off, y - off, brush, brush}, c);
}

void PixelRenderer::line(int x0, int y0, int x1, int y1, int brush, Color c) {
    const int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        stamp(x0, y0, brush, c);
        if (x0 == x1 && y0 == y1)
            break;
        const int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void PixelRenderer::drawLine(Point a, Point b, Color c, float thickness) {
    const int brush = brushOf(thickness);
    const int x0 = static_cast<int>(std::floor(a.x)), y0 = static_cast<int>(std::floor(a.y));
    const int x1 = static_cast<int>(std::floor(b.x)), y1 = static_cast<int>(std::floor(b.y));
    // Whole runs of a horizontal or vertical line are rectangles, and a translucent one is not blended twice.
    if (y0 == y1) {
        const int off = (brush - 1) / 2;
        fillRect({std::min(x0, x1) - off, y0 - off, std::abs(x1 - x0) + brush, brush}, c);
    } else if (x0 == x1) {
        const int off = (brush - 1) / 2;
        fillRect({x0 - off, std::min(y0, y1) - off, brush, std::abs(y1 - y0) + brush}, c);
    } else {
        line(x0, y0, x1, y1, brush, c);
    }
}

void PixelRenderer::drawPolyline(const Point* pts, int count, Color c, float thickness) {
    for (int i = 0; i + 1 < count; ++i)
        drawLine(pts[i], pts[i + 1], c, thickness);
}

void PixelRenderer::fillCircle(Point center, float radius, Color c) {
    const double cx = center.x, cy = center.y, r = radius;
    const Rect cl = clip();
    const int y0 = static_cast<int>(std::floor(cy - r)), y1 = static_cast<int>(std::ceil(cy + r));
    for (int y = y0; y <= y1; ++y) {
        int x0, x1;
        circleSpan(cx, cy, outerLimit(r), y, x0, x1);
        if (x1 >= x0)
            fillSpan(x0, x1 + 1, y, c, cl);
    }
}

void PixelRenderer::strokeCircle(Point center, float radius, Color c, float thickness) {
    const double cx = center.x, cy = center.y, r = radius;
    const double rin = r - static_cast<double>(brushOf(thickness));
    const Rect cl = clip();
    const int y0 = static_cast<int>(std::floor(cy - r)), y1 = static_cast<int>(std::ceil(cy + r));
    for (int y = y0; y <= y1; ++y) {
        int ox0, ox1, ix0 = 1, ix1 = 0;
        circleSpan(cx, cy, outerLimit(r), y, ox0, ox1);
        if (ox1 < ox0)
            continue;
        if (rin > 0.0)
            circleSpan(cx, cy, innerLimit(rin), y, ix0, ix1);
        if (ix1 < ix0) {
            fillSpan(ox0, ox1 + 1, y, c, cl);
        } else {
            fillSpan(ox0, ix0, y, c, cl);
            fillSpan(ix1 + 1, ox1 + 1, y, c, cl);
        }
    }
}

// --- text and bits -------------------------------------------------------------------------------------------

namespace {

// Rows of bits as runs of rectangles, so a scaled glyph is a handful of fills, not a pixel per bit.
template <class Fill> void drawRows(const std::uint8_t* rows, int w, int h, int x, int y, int scale, Fill&& fill) {
    for (int r = 0; r < h; ++r) {
        const std::uint8_t b = rows[r];
        if (!b)
            continue;
        for (int col = 0; col < w;) {
            if (!(b & (0x80 >> col))) {
                ++col;
                continue;
            }
            int run = 1;
            while (col + run < w && (b & (0x80 >> (col + run))))
                ++run;
            fill(Rect{x + col * scale, y + r * scale, run * scale, scale});
            col += run;
        }
    }
}

} // namespace

void PixelRenderer::bits(int x, int y, const std::uint8_t* rows, int w, int h, Color c, int scale) {
    scale = std::max(1, scale);
    if (!rows || !clip().overlaps({x, y, w * scale, h * scale}))
        return;
    drawRows(rows, std::min(w, 8), h, x, y, scale, [&](const Rect& r) { fillRect(r, c); });
}

void PixelRenderer::text(const BitmapFont& font, const char* s, int x, int capTop, Color c, int scale) {
    if (!s || !*s)
        return;
    scale = std::max(1, scale);
    const int y = capTop - font.capTop * scale;
    if (!clip().overlaps({x, y, font.textWidth(s, scale), font.height * scale}))
        return;
    const Rect cl = clip();
    for (const char* p = s; *p; ++p, x += font.advance * scale) {
        if (x >= cl.right())
            break;
        if (x + font.width * scale <= cl.x)
            continue;
        if (const std::uint8_t* g = font.glyph(*p))
            drawRows(g, font.width, font.height, x, y, scale, [&](const Rect& r) { fillRect(r, c); });
    }
}

void PixelRenderer::textCentered(const BitmapFont& font, const char* s, int centerX, int capTop, Color c, int scale) {
    text(font, s, centerX - font.textWidth(s, scale) / 2, capTop, c, scale);
}

void PixelRenderer::drawText(const char* s, int x, int y, Color c, int scale) { text(*font_, s, x, y, c, scale); }

void PixelRenderer::drawTextPx(const char* s, float x, float y, Color c, float px, Align align, FontWeight weight) {
    const BitmapFont& f = weight == FontWeight::Bold ? *bold_ : *font_;
    const int scale = bitmapScale(px);
    const int w = f.textWidth(s, scale);
    const int left = align == Align::Left     ? static_cast<int>(std::floor(x))
                     : align == Align::Center ? static_cast<int>(std::floor(x)) - w / 2
                                              : static_cast<int>(std::floor(x)) - w;
    text(f, s, left, static_cast<int>(std::floor(y)), c, scale);
}

float PixelRenderer::textWidthPx(const char* s, float px, FontWeight weight) const {
    return static_cast<float>((weight == FontWeight::Bold ? *bold_ : *font_).textWidth(s, bitmapScale(px)));
}

// --- effects ---------------------------------------------------------------------------------------------------

void PixelRenderer::drawArc(Point center, float radius, float a0, float a1, Color c, float thickness) {
    const int brush = brushOf(thickness);
    const double step = 0.5 / std::max(1.0, static_cast<double>(radius));
    const int n = std::max(1, static_cast<int>(std::ceil((static_cast<double>(a1) - a0) / step)));
    int lastX = 0, lastY = 0;
    for (int i = 0; i <= n; ++i) {
        const double a = static_cast<double>(a0) + (static_cast<double>(a1) - a0) * i / n;
        const int x = static_cast<int>(std::floor(center.x + std::cos(a) * radius));
        const int y = static_cast<int>(std::floor(center.y + std::sin(a) * radius));
        if (i > 0 && x == lastX && y == lastY)
            continue;
        stamp(x, y, brush, c);
        lastX = x;
        lastY = y;
    }
}

void PixelRenderer::drawGlow(Point center, float radius, Color c) {
    const Rect v =
        Rect{static_cast<int>(std::floor(center.x - radius)), static_cast<int>(std::floor(center.y - radius)),
             static_cast<int>(std::ceil(2.0f * radius)) + 1, static_cast<int>(std::ceil(2.0f * radius)) + 1}
            .intersect(clip());
    for (int y = v.y; y < v.bottom(); ++y)
        for (int x = v.x; x < v.right(); ++x) {
            const float d =
                std::hypot(static_cast<float>(x) + 0.5f - center.x, static_cast<float>(y) + 0.5f - center.y);
            const float t = d / std::max(radius, 1e-3f);
            if (t < 1.0f && dither(x, y, roundPx((1.0f - t) * (1.0f - t) * 16.0f)))
                pixel(x, y, c);
        }
}

void PixelRenderer::fillRadialGradient(Point center, float radius, Color inner, Color outer) {
    const Rect v =
        Rect{static_cast<int>(std::floor(center.x - radius)), static_cast<int>(std::floor(center.y - radius)),
             static_cast<int>(std::ceil(2.0f * radius)) + 1, static_cast<int>(std::ceil(2.0f * radius)) + 1}
            .intersect(clip());
    for (int y = v.y; y < v.bottom(); ++y)
        for (int x = v.x; x < v.right(); ++x) {
            const float d =
                std::hypot(static_cast<float>(x) + 0.5f - center.x, static_cast<float>(y) + 0.5f - center.y);
            if (d > radius)
                continue;
            // four steps from the inside out, the edge between two steps dithered
            const float t = d / std::max(radius, 1e-3f) * 3.0f;
            const float base = std::floor(t);
            const int up = dither(x, y, roundPx((t - base) * 16.0f)) ? 1 : 0;
            pixel(x, y, ui::mix(inner, outer, std::min(3.0f, base + static_cast<float>(up)) / 3.0f));
        }
}

void PixelRenderer::drawShadow(const Rect& r, float radius, float spread, Color c) {
    if (r.empty())
        return;
    const int w = std::clamp(roundPx(spread * 0.4f), 1, 8);
    const float R = static_cast<float>(std::clamp(roundPx(radius), 0, std::min(r.w, r.h) / 2));
    const Point mid = r.center();
    const float hw = static_cast<float>(r.w) * 0.5f, hh = static_cast<float>(r.h) * 0.5f;
    const Rect v = r.expand(Margin::all(w)).intersect(clip());
    for (int y = v.y; y < v.bottom(); ++y)
        for (int x = v.x; x < v.right(); ++x) {
            // the signed distance to the rounded rectangle, negative inside
            const float qx = std::fabs(static_cast<float>(x) + 0.5f - mid.x) - (hw - R);
            const float qy = std::fabs(static_cast<float>(y) + 0.5f - mid.y) - (hh - R);
            const float d = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - R;
            if (d >= static_cast<float>(w))
                continue;
            const int level = d <= 0.0f ? 16 : roundPx((1.0f - d / static_cast<float>(w)) * 16.0f);
            if (dither(x, y, level))
                pixel(x, y, c);
        }
}

void PixelRenderer::drawPanel(const Rect& r, float /*radius*/, Color tint) {
    fillRect(r, tint);
    bevel(r, withAlpha(0xFFFFFFFFu, 0.3f), withAlpha(0xFF000000u, 0.3f));
}

void PixelRenderer::drawMeter(const Rect& area, float level, Color c) {
    constexpr int kSegment = 3, kGap = 1;
    const int count = (area.h + kGap) / (kSegment + kGap);
    const float l = level < 0.0f ? 0.0f : level > 1.0f ? 1.0f : level;
    const int lit = roundPx(l * static_cast<float>(count));
    for (int i = 0; i < count; ++i)
        fillRect({area.x, area.bottom() - (i + 1) * (kSegment + kGap) + kGap, area.w, kSegment},
                 i < lit ? c : withAlpha(c, 0.2f));
}

} // namespace spux::inline v1::ui::retro
