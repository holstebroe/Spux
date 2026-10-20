#include "spux/ui/modern/SoftRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace spux::inline v1::ui::modern {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;

float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
float smoothstep(float a, float b, float x) {
    const float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

// Straight-alpha interpolation of all four channels.
Color lerpColor(Color a, Color b, float t) {
    auto ch = [&](int sh) {
        const float x = static_cast<float>((a >> sh) & 255u), y = static_cast<float>((b >> sh) & 255u);
        return static_cast<Color>(x + (y - x) * t + 0.5f) << sh;
    };
    return ch(24) | ch(16) | ch(8) | ch(0);
}

// Bilinear sample of the distance field at atlas coordinates, 0..1.
float sampleAtlas(const std::vector<std::uint8_t>& px, float u, float v) {
    constexpr int n = FontAtlas::kSize;
    const float fx = u * static_cast<float>(n) - 0.5f, fy = v * static_cast<float>(n) - 0.5f;
    const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
    const float tx = fx - static_cast<float>(x0), ty = fy - static_cast<float>(y0);
    auto at = [&](int x, int y) {
        x = std::clamp(x, 0, n - 1);
        y = std::clamp(y, 0, n - 1);
        return static_cast<float>(px[static_cast<std::size_t>(y) * n + static_cast<std::size_t>(x)]);
    };
    const float top = at(x0, y0) + (at(x0 + 1, y0) - at(x0, y0)) * tx;
    const float bottom = at(x0, y0 + 1) + (at(x0 + 1, y0 + 1) - at(x0, y0 + 1)) * tx;
    return (top + (bottom - top) * ty) * (1.0f / 255.0f);
}

} // namespace

SoftRenderer::SoftRenderer(int width, int height, int supersample, std::shared_ptr<const FontAtlas> font)
    : layers_(std::max(1, width), std::max(1, height), std::clamp(supersample, 1, 4)), font_(std::move(font)) {
    clips_.reserve(16);
    if (!font_)
        font_ = FontAtlas::shared();
    if (!font_)
        font_ = makeFontAtlas();
}

std::shared_ptr<const FontAtlas> SoftRenderer::makeFontAtlas() {
    auto atlas = std::make_shared<FontAtlas>();
    atlas->build();
    return atlas;
}

std::vector<std::uint32_t> SoftRenderer::frame() const {
    std::vector<std::uint32_t> out(static_cast<std::size_t>(width()) * static_cast<std::size_t>(height()));
    layers_.resolve(out.data(), {0, 0, width(), height()});
    return out;
}

// --- plumbing --------------------------------------------------------------------------------------

SoftRenderer::Hi SoftRenderer::clipHi() const {
    const int s = layers_.scale();
    const Rect c = clips_.empty() ? Rect{0, 0, width(), height()} : clips_.back();
    return {c.x * s, c.y * s, c.right() * s, c.bottom() * s};
}

SoftRenderer::Hi SoftRenderer::boxHi(float x0, float y0, float x1, float y1) const {
    const Hi c = clipHi();
    const float s = static_cast<float>(layers_.scale());
    return {std::max(c.x0, static_cast<int>(std::floor(x0 * s))), std::max(c.y0, static_cast<int>(std::floor(y0 * s))),
            std::min(c.x1, static_cast<int>(std::ceil(x1 * s))), std::min(c.y1, static_cast<int>(std::ceil(y1 * s)))};
}

// Blends one supersampled pixel. The caller has kept hx, hy inside the clip, which lies inside the buffer.
void SoftRenderer::put(int hx, int hy, Color c, float coverage) {
    const float a = static_cast<float>(c >> 24) * (1.0f / 255.0f) * coverage;
    if (a <= 0.0f)
        return;
    std::uint32_t& d = layers_.target()[static_cast<std::size_t>(hy) * static_cast<std::size_t>(layers_.rowPixels()) +
                                        static_cast<std::size_t>(hx)];
    if (blend_ == Blend::Alpha) {
        if (a >= 0.999f) {
            d = c | 0xFF000000u;
            return;
        }
        auto mixCh = [a, c, d](int sh) {
            const float s = static_cast<float>((c >> sh) & 255u), t = static_cast<float>((d >> sh) & 255u);
            return static_cast<std::uint32_t>(s * a + t * (1.0f - a) + 0.5f) << sh;
        };
        d = 0xFF000000u | mixCh(16) | mixCh(8) | mixCh(0);
    } else {
        auto addCh = [a, c, d](int sh) {
            const float s = static_cast<float>((c >> sh) & 255u), t = static_cast<float>((d >> sh) & 255u);
            return static_cast<std::uint32_t>(std::min(255.0f, t + s * a) + 0.5f) << sh;
        };
        d = 0xFF000000u | addCh(16) | addCh(8) | addCh(0);
    }
}

void SoftRenderer::clear(Color c) {
    std::uint32_t* p = layers_.target();
    std::fill(p,
              p + static_cast<std::size_t>(layers_.rowPixels()) * static_cast<std::size_t>(height() * layers_.scale()),
              c | 0xFF000000u);
}

void SoftRenderer::pushClip(const Rect& r) {
    const Rect base = clips_.empty() ? Rect{0, 0, width(), height()} : clips_.back();
    clips_.push_back(base.intersect(r));
}

void SoftRenderer::popClip() {
    if (!clips_.empty())
        clips_.pop_back();
}

// --- boxes -------------------------------------------------------------------------------------------

// A rounded box (fill, or a stroke of `stroke` pixels inside the edge) with a vertical color gradient.
void SoftRenderer::box(const Rect& r, float radius, float stroke, Color top, Color bottom) {
    if (r.empty())
        return;
    const Hi b = boxHi(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.right()),
                       static_cast<float>(r.bottom()));
    const float s = static_cast<float>(layers_.scale()), inv = 1.0f / s;
    const float hw = static_cast<float>(r.w) * 0.5f, hh = static_cast<float>(r.h) * 0.5f;
    radius = std::clamp(radius, 0.0f, std::min(hw, hh));
    const float cx = static_cast<float>(r.x) + hw, cy = static_cast<float>(r.y) + hh;
    const bool flat = top == bottom;
    for (int y = b.y0; y < b.y1; ++y) {
        const float py = (static_cast<float>(y) + 0.5f) * inv - cy;
        const Color rowColor = flat ? top : lerpColor(top, bottom, clamp01((py + hh) / (2.0f * hh)));
        const float qy = std::fabs(py) - (hh - radius);
        for (int x = b.x0; x < b.x1; ++x) {
            const float px = (static_cast<float>(x) + 0.5f) * inv - cx;
            const float qx = std::fabs(px) - (hw - radius);
            float d = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - radius;
            if (stroke > 0.0f)
                d = std::fabs(d + stroke * 0.5f) - stroke * 0.5f;
            put(x, y, rowColor, clamp01(0.5f - d * s));
        }
    }
}

void SoftRenderer::fillRect(const Rect& r, Color c) {
    const Hi b = boxHi(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.right()),
                       static_cast<float>(r.bottom()));
    for (int y = b.y0; y < b.y1; ++y)
        for (int x = b.x0; x < b.x1; ++x)
            put(x, y, c, 1.0f);
}

void SoftRenderer::strokeRect(const Rect& r, Color c, float thickness) { box(r, 0.0f, thickness, c, c); }
void SoftRenderer::fillRoundRect(const Rect& r, float radius, Color c) { box(r, radius, 0.0f, c, c); }
void SoftRenderer::strokeRoundRect(const Rect& r, float radius, Color c, float thickness) {
    box(r, radius, thickness, c, c);
}
void SoftRenderer::fillGradientRect(const Rect& r, Color top, Color bottom) { box(r, 0.0f, 0.0f, top, bottom); }

void SoftRenderer::drawPanel(const Rect& r, float radius, Color tint) {
    box(r, radius, 0.0f, mix(tint, 0xFFFFFFFFu, 0.07f), tint);
    box(r, radius, 1.0f, withAlpha(0xFFFFFFFFu, 0.22f), withAlpha(0xFFFFFFFFu, 0.05f));
}

// --- lines and strips ----------------------------------------------------------------------------------

void SoftRenderer::drawLine(Point a, Point b, Color c, float thickness) {
    const Point pts[2] = {a, b};
    const Color cols[2] = {c, c};
    const float ws[2] = {thickness, thickness};
    strip(pts, cols, ws, 2);
}

void SoftRenderer::drawPolyline(const Point* pts, int count, Color c, float thickness) {
    if (count < 2)
        return;
    std::vector<Color> cols(static_cast<std::size_t>(count), c);
    std::vector<float> ws(static_cast<std::size_t>(count), thickness);
    strip(pts, cols.data(), ws.data(), count);
}

void SoftRenderer::drawStrip(const Point* pts, const Color* colors, const float* widths, int count) {
    strip(pts, colors, widths, count);
}

// Every pixel takes the segment that covers it best, so a translucent line does not darken where two
// segments overlap at a joint. Width and color are interpolated along each segment.
void SoftRenderer::strip(const Point* pts, const Color* colors, const float* widths, int count) {
    if (count < 2)
        return;
    float x0 = pts[0].x, y0 = pts[0].y, x1 = x0, y1 = y0, wmax = 0.0f;
    for (int i = 0; i < count; ++i) {
        x0 = std::min(x0, pts[i].x);
        y0 = std::min(y0, pts[i].y);
        x1 = std::max(x1, pts[i].x);
        y1 = std::max(y1, pts[i].y);
        wmax = std::max(wmax, widths[i]);
    }
    const float pad = std::max(wmax, 0.5f) * 0.5f + 1.0f;
    const Hi b = boxHi(x0 - pad, y0 - pad, x1 + pad, y1 + pad);
    const int bw = b.x1 - b.x0, bh = b.y1 - b.y0;
    if (bw <= 0 || bh <= 0)
        return;
    const std::size_t n = static_cast<std::size_t>(bw) * static_cast<std::size_t>(bh);
    maskCover_.assign(n, 0.0f);
    maskColor_.assign(n, 0u);
    const float s = static_cast<float>(layers_.scale()), inv = 1.0f / s;
    for (int i = 0; i + 1 < count; ++i) {
        const Point a = pts[i], e = pts[i + 1];
        const float hwA = std::max(widths[i], 0.5f) * 0.5f, hwB = std::max(widths[i + 1], 0.5f) * 0.5f;
        const float m = std::max(hwA, hwB) + 1.0f;
        const Hi sb =
            boxHi(std::min(a.x, e.x) - m, std::min(a.y, e.y) - m, std::max(a.x, e.x) + m, std::max(a.y, e.y) + m);
        const float dx = e.x - a.x, dy = e.y - a.y, len2 = dx * dx + dy * dy;
        for (int y = std::max(sb.y0, b.y0); y < std::min(sb.y1, b.y1); ++y)
            for (int x = std::max(sb.x0, b.x0); x < std::min(sb.x1, b.x1); ++x) {
                const float px = (static_cast<float>(x) + 0.5f) * inv, py = (static_cast<float>(y) + 0.5f) * inv;
                const float t = len2 > 0.0f ? clamp01(((px - a.x) * dx + (py - a.y) * dy) / len2) : 0.0f;
                const float d = std::hypot(px - (a.x + t * dx), py - (a.y + t * dy));
                const float half = hwA + (hwB - hwA) * t;
                const float cov = clamp01((half - d) * s + 0.5f);
                const std::size_t k = static_cast<std::size_t>(y - b.y0) * static_cast<std::size_t>(bw) +
                                      static_cast<std::size_t>(x - b.x0);
                if (cov > maskCover_[k]) {
                    maskCover_[k] = cov;
                    maskColor_[k] = lerpColor(colors[i], colors[i + 1], t);
                }
            }
    }
    for (int y = 0; y < bh; ++y)
        for (int x = 0; x < bw; ++x) {
            const std::size_t k =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(bw) + static_cast<std::size_t>(x);
            if (maskCover_[k] > 0.0f)
                put(b.x0 + x, b.y0 + y, maskColor_[k], maskCover_[k]);
        }
}

void SoftRenderer::fillUnder(const Point* pts, int count, float baselineY, Color top, Color bottom) {
    if (count < 2)
        return;
    const float s = static_cast<float>(layers_.scale()), inv = 1.0f / s;
    float xa = pts[0].x, xb = pts[count - 1].x;
    float ymin = pts[0].y;
    for (int i = 0; i < count; ++i)
        ymin = std::min(ymin, pts[i].y);
    const Hi b = boxHi(xa, ymin - 1.0f, xb, baselineY);
    int seg = 0;
    for (int x = b.x0; x < b.x1; ++x) {
        const float px = (static_cast<float>(x) + 0.5f) * inv;
        while (seg + 2 < count && pts[seg + 1].x < px)
            ++seg;
        const Point p = pts[seg], q = pts[seg + 1];
        if (px < p.x || px > q.x)
            continue;
        const float u = q.x > p.x ? (px - p.x) / (q.x - p.x) : 0.0f;
        const float yLine = p.y + (q.y - p.y) * u;
        const float span = std::max(baselineY - yLine, 1e-3f);
        for (int y = std::max(b.y0, static_cast<int>(std::floor((yLine - 0.5f) * s))); y < b.y1; ++y) {
            const float py = (static_cast<float>(y) + 0.5f) * inv;
            const float cov = clamp01((py - yLine) * s + 0.5f);
            if (cov > 0.0f && py < baselineY)
                put(x, y, lerpColor(top, bottom, clamp01((py - yLine) / span)), cov);
        }
    }
}

// --- circles, arcs, glow -------------------------------------------------------------------------------

void SoftRenderer::fillCircle(Point p, float radius, Color c) {
    const Hi b = boxHi(p.x - radius - 1.0f, p.y - radius - 1.0f, p.x + radius + 1.0f, p.y + radius + 1.0f);
    const float s = static_cast<float>(layers_.scale()), inv = 1.0f / s;
    for (int y = b.y0; y < b.y1; ++y)
        for (int x = b.x0; x < b.x1; ++x) {
            const float d =
                std::hypot((static_cast<float>(x) + 0.5f) * inv - p.x, (static_cast<float>(y) + 0.5f) * inv - p.y);
            put(x, y, c, clamp01((radius - d) * s + 0.5f));
        }
}

void SoftRenderer::strokeCircle(Point p, float radius, Color c, float thickness) {
    const float half = std::max(thickness, 0.25f) * 0.5f;
    const Hi b = boxHi(p.x - radius - half - 1.0f, p.y - radius - half - 1.0f, p.x + radius + half + 1.0f,
                       p.y + radius + half + 1.0f);
    const float s = static_cast<float>(layers_.scale()), inv = 1.0f / s;
    for (int y = b.y0; y < b.y1; ++y)
        for (int x = b.x0; x < b.x1; ++x) {
            const float d =
                std::hypot((static_cast<float>(x) + 0.5f) * inv - p.x, (static_cast<float>(y) + 0.5f) * inv - p.y);
            put(x, y, c, clamp01((half - std::fabs(d - radius)) * s + 0.5f));
        }
}

void SoftRenderer::drawArc(Point center, float radius, float a0, float a1, Color c, float thickness) {
    if (a1 <= a0)
        return;
    a1 = std::min(a1, a0 + kTwoPi);
    const float half = std::max(thickness, 0.5f) * 0.5f;
    const float e = radius + half + 1.0f;
    const Hi b = boxHi(center.x - e, center.y - e, center.x + e, center.y + e);
    const float s = static_cast<float>(layers_.scale()), inv = 1.0f / s;
    const Point e0{center.x + std::cos(a0) * radius, center.y + std::sin(a0) * radius};
    const Point e1{center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius};
    for (int y = b.y0; y < b.y1; ++y)
        for (int x = b.x0; x < b.x1; ++x) {
            const float px = (static_cast<float>(x) + 0.5f) * inv - center.x;
            const float py = (static_cast<float>(y) + 0.5f) * inv - center.y;
            float a = std::fmod(std::atan2(py, px) - a0, kTwoPi);
            if (a < 0.0f)
                a += kTwoPi;
            float d;
            if (a <= a1 - a0)
                d = std::fabs(std::hypot(px, py) - radius) - half;
            else
                d = std::min(std::hypot(px + center.x - e0.x, py + center.y - e0.y),
                             std::hypot(px + center.x - e1.x, py + center.y - e1.y)) -
                    half;
            put(x, y, c, clamp01(0.5f - d * s));
        }
}

void SoftRenderer::drawGlow(Point center, float radius, Color c) {
    const Hi b = boxHi(center.x - radius, center.y - radius, center.x + radius, center.y + radius);
    const float inv = 1.0f / static_cast<float>(layers_.scale());
    for (int y = b.y0; y < b.y1; ++y)
        for (int x = b.x0; x < b.x1; ++x) {
            const float t = std::hypot((static_cast<float>(x) + 0.5f) * inv - center.x,
                                       (static_cast<float>(y) + 0.5f) * inv - center.y) /
                            radius;
            put(x, y, c, std::exp(-t * t * 3.2f) * (1.0f - smoothstep(0.85f, 1.0f, t)));
        }
}

void SoftRenderer::fillRadialGradient(Point center, float radius, Color inner, Color outer) {
    const Hi b =
        boxHi(center.x - radius - 1.0f, center.y - radius - 1.0f, center.x + radius + 1.0f, center.y + radius + 1.0f);
    const float s = static_cast<float>(layers_.scale()), inv = 1.0f / s;
    for (int y = b.y0; y < b.y1; ++y)
        for (int x = b.x0; x < b.x1; ++x) {
            const float d = std::hypot((static_cast<float>(x) + 0.5f) * inv - center.x,
                                       (static_cast<float>(y) + 0.5f) * inv - center.y);
            put(x, y, lerpColor(inner, outer, clamp01(d / radius)), clamp01((radius - d) * s + 0.5f));
        }
}

// --- text ----------------------------------------------------------------------------------------------

float SoftRenderer::textWidthPx(const char* text, float px, FontWeight weight) const {
    return text ? font_->width(weight, text) * px : 0.0f;
}
int SoftRenderer::textWidth(const char* text, int scale) const {
    return static_cast<int>(std::ceil(textWidthPx(text, 9.0f * static_cast<float>(scale))));
}
int SoftRenderer::textHeight(int scale) const {
    return static_cast<int>(std::ceil(FontAtlas::kCapHeight * 9.0f * static_cast<float>(scale)));
}
void SoftRenderer::drawText(const char* text, int x, int y, Color c, int scale) {
    drawTextPx(text, static_cast<float>(x), static_cast<float>(y), c, 9.0f * static_cast<float>(scale));
}

// The GPU shader's text, per supersampled pixel: bilinear distance, a smoothstep around the outline whose
// width follows how fast the distance changes between neighboring pixels.
void SoftRenderer::drawTextPx(const char* text, float x, float y, Color c, float px, Align align, FontWeight weight) {
    if (!text || !*text)
        return;
    const float w = textWidthPx(text, px, weight);
    if (align == Align::Center)
        x -= w * 0.5f;
    else if (align == Align::Right)
        x -= w;
    const float s = static_cast<float>(layers_.scale()), inv = 1.0f / s;
    const float baseline = y + FontAtlas::kCapHeight * px;
    // small text is thickened a little: a hairline stem at 10 px would drop out of the distance field
    const float small = std::max(0.0f, 14.0f - px);
    const float edge = 0.5f - ((weight == FontWeight::Bold ? 0.0f : 0.02f) + small * 0.012f);
    const std::vector<std::uint8_t>& atlas = font_->pixels();
    float pen = x;
    const std::string_view sv(text);
    for (std::size_t i = 0; i < sv.size();) {
        const FontAtlas::Glyph& g = font_->glyph(weight, FontAtlas::nextCodePoint(sv, i));
        if (g.valid) {
            const float gx0 = pen + g.x0 * px, gx1 = pen + g.x1 * px;
            const float gy0 = baseline + g.y0 * px, gy1 = baseline + g.y1 * px;
            const Hi b = boxHi(gx0, gy0, gx1, gy1);
            const float du = (g.u1 - g.u0) / (gx1 - gx0), dv = (g.v1 - g.v0) / (gy1 - gy0);
            const float stepU = du * inv, stepV = dv * inv; // one supersampled pixel in atlas coordinates
            for (int hy = b.y0; hy < b.y1; ++hy)
                for (int hx = b.x0; hx < b.x1; ++hx) {
                    const float u = g.u0 + ((static_cast<float>(hx) + 0.5f) * inv - gx0) * du;
                    const float v = g.v0 + ((static_cast<float>(hy) + 0.5f) * inv - gy0) * dv;
                    const float d = sampleAtlas(atlas, u, v);
                    const float fw = std::fabs(sampleAtlas(atlas, u + stepU, v) - d) +
                                     std::fabs(sampleAtlas(atlas, u, v + stepV) - d);
                    const float aw = std::max(fw * 0.7f, 0.012f);
                    put(hx, hy, c, smoothstep(edge - aw, edge + aw, d));
                }
        }
        pen += g.advance * px;
    }
}

// A static stand-in for the GPU's animated backdrop: a dark vertical gradient with a faint tint of each
// accent in two opposite corners.
void SoftRenderer::drawBackground(Color a, Color b, float) {
    const int w = width(), h = height();
    const float reach = 0.7f * static_cast<float>(std::max(w, h));
    box({0, 0, w, h}, 0.0f, 0.0f, rgb(22, 26, 41), rgb(10, 13, 26));
    fillRadialGradient({0.18f * static_cast<float>(w), 0.15f * static_cast<float>(h)}, reach, withAlpha(a, 0.10f),
                       withAlpha(a, 0.0f));
    fillRadialGradient({0.85f * static_cast<float>(w), 0.85f * static_cast<float>(h)}, reach, withAlpha(b, 0.08f),
                       withAlpha(b, 0.0f));
}

// --- composed shapes -----------------------------------------------------------------------------------

void SoftRenderer::drawTrace(const float* values, int n, const Rect& area, float lo, float hi, Color c) {
    if (n < 2 || area.empty() || hi == lo)
        return;
    std::vector<Point> pts(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const float t = clamp01((values[i] - lo) / (hi - lo));
        pts[static_cast<std::size_t>(i)] = {
            static_cast<float>(area.x) + static_cast<float>(area.w) * static_cast<float>(i) / static_cast<float>(n - 1),
            static_cast<float>(area.bottom()) - t * static_cast<float>(area.h)};
    }
    fillUnder(pts.data(), n, static_cast<float>(area.bottom()), withAlpha(c, 0.28f), withAlpha(c, 0.0f));
    drawPolyline(pts.data(), n, c, 1.6f);
}

void SoftRenderer::drawMeter(const Rect& area, float level, Color c) {
    if (area.empty())
        return;
    const float r = static_cast<float>(std::min(area.w, area.h)) * 0.5f;
    box(area, r, 0.0f, withAlpha(0xFFFFFFFFu, 0.06f), withAlpha(0xFFFFFFFFu, 0.04f));
    const int fh = static_cast<int>(std::lround(static_cast<float>(area.h) * clamp01(level)));
    if (fh >= 1)
        box({area.x, area.bottom() - fh, area.w, fh}, r, 0.0f, c, mix(c, 0xFF000000u, 0.45f));
}

} // namespace spux::inline v1::ui::modern
