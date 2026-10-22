#include "spux/ui/classic/ShadedRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

// The font data lives in this translation unit only: one copy in a program, whoever draws with it.
#include "spux/ui/classic/DisplayFont.hpp"
#include "spux/ui/classic/LabelFont.hpp"

namespace spux::inline v1::ui::classic {

namespace {

constexpr float kCapPerPx = 0.7f;  // capitals of a font at a pixel size (drawTextPx)
constexpr float kBitmapCap = 6.0f; // cap height of drawText at scale 1

} // namespace

ShadedRenderer::ShadedRenderer(int width, int height, int supersample)
    : layers_(std::max(1, width), std::max(1, height), std::clamp(supersample, 1, 4)) {
    clips_.reserve(16);
}

const AtlasFont& ShadedRenderer::labelFont() { return kLabelFont; }
const AtlasFont& ShadedRenderer::displayFont() { return kDisplayFont; }

std::vector<std::uint32_t> ShadedRenderer::frame() const {
    std::vector<std::uint32_t> out(static_cast<std::size_t>(width()) * static_cast<std::size_t>(height()));
    layers_.resolve(out.data(), {0, 0, width(), height()});
    return out;
}

Graphics ShadedRenderer::graphics() {
    const int s = layers_.scale();
    Graphics g(layers_.target(), static_cast<std::uint32_t>(width()), static_cast<std::uint32_t>(height()), s);
    if (!clips_.empty()) {
        const Rect& c = clips_.back();
        g.setClip(c.x * s, c.y * s, c.right() * s, c.bottom() * s);
    }
    return g;
}

void ShadedRenderer::pushClip(const Rect& r) {
    const Rect base = clips_.empty() ? Rect{0, 0, width(), height()} : clips_.back();
    clips_.push_back(base.intersect(r));
}

void ShadedRenderer::popClip() {
    if (!clips_.empty())
        clips_.pop_back();
}

void ShadedRenderer::clear(Argb c) {
    std::uint32_t* p = layers_.target();
    std::fill(p,
              p + static_cast<std::size_t>(layers_.rowPixels()) * static_cast<std::size_t>(height() * layers_.scale()),
              c | 0xFF000000u);
}

// --- shapes ----------------------------------------------------------------------------------------------

namespace {

// Paints a rounded box (or, with stroke > 0, a ring of that width just inside its edge) with a vertical
// gradient from `top` to `bottom`.
void paintBox(Graphics& g, const Rect& r, float radius, float stroke, Argb top, Argb bottom) {
    if (r.empty())
        return;
    const float s = static_cast<float>(g.getScale());
    const float x0 = static_cast<float>(r.x), y0 = static_cast<float>(r.y);
    const float hx = static_cast<float>(r.w) * 0.5f, hy = static_cast<float>(r.h) * 0.5f;
    const float cx = x0 + hx, cy = y0 + hy;
    const float rad = std::clamp(radius, 0.0f, std::min(hx, hy));
    const float invH = 1.0f / static_cast<float>(r.h);
    shadeBox(g, x0 - 1, y0 - 1, x0 + 2 * hx + 1, y0 + 2 * hy + 1, [&](float x, float y) {
        const float d = sdRoundRect(x - cx, y - cy, hx, hy, rad);
        const float edge = stroke > 0.0f ? std::fabs(d + 0.5f * stroke) - 0.5f * stroke : d;
        const float cov = clamp01(0.5f - edge * s);
        if (cov <= 0.0f)
            return 0u;
        const Argb c = top == bottom ? top : ui::mix(top, bottom, clamp01((y - y0) * invH));
        return toArgb(fromArgb(c), alphaOf(c) * cov);
    });
}

} // namespace

void ShadedRenderer::fillRect(const Rect& r, Argb c) {
    const int s = layers_.scale();
    Graphics g = graphics();
    const int x0 = std::max(g.clipX0(), r.x * s), x1 = std::min(g.clipX1(), r.right() * s);
    const int y0 = std::max(g.clipY0(), r.y * s), y1 = std::min(g.clipY1(), r.bottom() * s);
    if (x1 <= x0 || y1 <= y0)
        return;
    std::uint32_t* base = layers_.target();
    const std::size_t row = static_cast<std::size_t>(layers_.rowPixels());
    for (int y = y0; y < y1; ++y) {
        std::uint32_t* p = base + static_cast<std::size_t>(y) * row;
        if ((c >> 24) == 0xFF)
            std::fill(p + x0, p + x1, c);
        else
            for (int x = x0; x < x1; ++x)
                g.blendPixel(x, y, c);
    }
}

void ShadedRenderer::strokeRect(const Rect& r, Argb c, float thickness) {
    Graphics g = graphics();
    paintBox(g, r, 0.0f, thickness, c, c);
}

void ShadedRenderer::fillRoundRect(const Rect& r, float radius, Argb c) {
    Graphics g = graphics();
    paintBox(g, r, radius, 0.0f, c, c);
}

void ShadedRenderer::fillGradientRect(const Rect& r, Argb top, Argb bottom) {
    Graphics g = graphics();
    paintBox(g, r, 0.0f, 0.0f, top, bottom);
}

void ShadedRenderer::strokeRoundRect(const Rect& r, float radius, Argb c, float thickness) {
    Graphics g = graphics();
    paintBox(g, r, radius, thickness, c, c);
}

void ShadedRenderer::drawLine(Point a, Point b, Argb c, float thickness) {
    Graphics g = graphics();
    drawLineAA(g, a.x, a.y, b.x, b.y, thickness, c);
}

void ShadedRenderer::drawPolyline(const Point* pts, int count, Argb c, float thickness) {
    Graphics g = graphics();
    for (int i = 0; i + 1 < count; ++i)
        drawLineAA(g, pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, thickness, c);
}

void ShadedRenderer::fillCircle(Point center, float radius, Argb c) {
    Graphics g = graphics();
    const float s = static_cast<float>(g.getScale());
    shadeBox(g, center.x - radius - 1, center.y - radius - 1, center.x + radius + 1, center.y + radius + 1,
             [&](float x, float y) {
                 const float d = std::hypot(x - center.x, y - center.y) - radius;
                 const float cov = clamp01(0.5f - d * s);
                 return cov > 0.0f ? toArgb(fromArgb(c), alphaOf(c) * cov) : 0u;
             });
}

void ShadedRenderer::strokeCircle(Point center, float radius, Argb c, float thickness) {
    Graphics g = graphics();
    const float s = static_cast<float>(g.getScale());
    const float mid = radius - 0.5f * thickness;
    shadeBox(g, center.x - radius - 1, center.y - radius - 1, center.x + radius + 1, center.y + radius + 1,
             [&](float x, float y) {
                 const float d = std::fabs(std::hypot(x - center.x, y - center.y) - mid) - 0.5f * thickness;
                 const float cov = clamp01(0.5f - d * s);
                 return cov > 0.0f ? toArgb(fromArgb(c), alphaOf(c) * cov) : 0u;
             });
}

void ShadedRenderer::drawGlow(Point center, float radius, Argb c) {
    Graphics g = graphics();
    shadeBox(g, center.x - radius, center.y - radius, center.x + radius, center.y + radius, [&](float x, float y) {
        const float t = std::hypot(x - center.x, y - center.y) / std::max(radius, 1e-3f);
        const float a = alphaOf(c) * (1.0f - smoothstep(0.0f, 1.0f, t));
        return a > 0.002f ? toArgb(fromArgb(c), a) : 0u;
    });
}

void ShadedRenderer::fillRadialGradient(Point center, float radius, Argb inner, Argb outer) {
    Graphics g = graphics();
    const float s = static_cast<float>(g.getScale());
    shadeBox(g, center.x - radius - 1, center.y - radius - 1, center.x + radius + 1, center.y + radius + 1,
             [&](float x, float y) {
                 const float d = std::hypot(x - center.x, y - center.y);
                 const float cov = clamp01((radius - d) * s + 0.5f);
                 if (cov <= 0.0f)
                     return 0u;
                 const Argb c = ui::mix(inner, outer, clamp01(d / std::max(radius, 1e-3f)));
                 return toArgb(fromArgb(c), alphaOf(c) * cov);
             });
}

void ShadedRenderer::drawShadow(const Rect& r, float radius, float spread, Argb c) {
    Graphics g = graphics();
    const float cx = r.center().x, cy = r.center().y;
    const float hx = static_cast<float>(r.w) * 0.5f, hy = static_cast<float>(r.h) * 0.5f;
    const float rad = std::clamp(radius, 0.0f, std::min(hx, hy));
    const Color rgbc = fromArgb(c);
    const float a0 = alphaOf(c);
    shadeBox(g, static_cast<float>(r.x) - spread, static_cast<float>(r.y) - spread,
             static_cast<float>(r.right()) + spread, static_cast<float>(r.bottom()) + spread, [&](float x, float y) {
                 const float d = sdRoundRect(x - cx, y - cy, hx, hy, rad);
                 const float a = a0 * (1.0f - smoothstep(-spread * 0.3f, spread, d));
                 return a > 0.002f ? toArgb(rgbc, a) : 0u;
             });
}

// --- text ---------------------------------------------------------------------------------------------------

void ShadedRenderer::ink(const AtlasFont& font, const char* text, float x, float capTop, float cap, Argb c,
                         float wear) {
    Graphics g = graphics();
    classic::drawText(g, font, text, x, capTop, cap, c, wear);
}

void ShadedRenderer::inkCentered(const AtlasFont& font, const char* text, float centerX, float capTop, float cap,
                                 Argb c, float wear) {
    ink(font, text, centerX - classic::textWidth(font, text, cap) * 0.5f, capTop, cap, c, wear);
}

void ShadedRenderer::drawText(const char* text, int x, int y, Argb c, int scale) {
    ink(kLabelFont, text, static_cast<float>(x), static_cast<float>(y), kBitmapCap * static_cast<float>(scale), c);
}

int ShadedRenderer::textWidth(const char* text, int scale) const {
    return static_cast<int>(std::ceil(classic::textWidth(kLabelFont, text, kBitmapCap * static_cast<float>(scale))));
}

int ShadedRenderer::textHeight(int scale) const { return static_cast<int>(kBitmapCap) * scale; }

void ShadedRenderer::drawTextPx(const char* text, float x, float y, Argb c, float px, Align align, FontWeight weight) {
    const AtlasFont& font = weight == FontWeight::Bold ? kDisplayFont : kLabelFont;
    const float cap = px * kCapPerPx;
    const float w = classic::textWidth(font, text, cap);
    const float left = align == Align::Left ? x : align == Align::Center ? x - 0.5f * w : x - w;
    ink(font, text, left, y, cap, c);
}

float ShadedRenderer::textWidthPx(const char* text, float px, FontWeight weight) const {
    return classic::textWidth(weight == FontWeight::Bold ? kDisplayFont : kLabelFont, text, px * kCapPerPx);
}

// --- the worn panel ---------------------------------------------------------------------------------------

void ShadedRenderer::wornPanel(const Rect& r, bool trims) {
    PanelKey key;
    key.kind = 1;
    key.w = r.w;
    key.h = r.h;
    key.scale = layers_.scale();
    key.trims = trims;
    paintPanelTexture(r, key);
}

void ShadedRenderer::brushedPanel(const Rect& r, Color light, Color dark, const BrushedSteel& steel) {
    PanelKey key;
    key.kind = 2;
    key.w = r.w;
    key.h = r.h;
    key.scale = layers_.scale();
    key.light = light;
    key.dark = dark;
    key.steel = steel;
    paintPanelTexture(r, key);
}

// Renders the texture when the cache does not hold this one, then copies it into the target, clipped.
void ShadedRenderer::paintPanelTexture(const Rect& r, const PanelKey& key) {
    if (r.empty())
        return;
    const int s = layers_.scale();
    if (!(panelKey_ == key)) {
        worn_.assign(static_cast<std::size_t>(r.w) * static_cast<std::size_t>(r.h) * static_cast<std::size_t>(s * s),
                     0);
        Graphics g(worn_.data(), static_cast<std::uint32_t>(r.w), static_cast<std::uint32_t>(r.h), s);
        if (key.kind == 1)
            paintWornPanel(g, r.w, r.h, key.trims);
        else
            paintBrushedPanel(g, r.w, r.h, key.light, key.dark, key.steel);
        panelKey_ = key;
    }
    const Graphics g = graphics();
    const int x0 = std::max(g.clipX0(), r.x * s), x1 = std::min(g.clipX1(), r.right() * s);
    const int y0 = std::max(g.clipY0(), r.y * s), y1 = std::min(g.clipY1(), r.bottom() * s);
    if (x1 <= x0 || y1 <= y0)
        return;
    std::uint32_t* base = layers_.target();
    const std::size_t row = static_cast<std::size_t>(layers_.rowPixels());
    const std::size_t srcRow = static_cast<std::size_t>(r.w) * static_cast<std::size_t>(s);
    for (int y = y0; y < y1; ++y)
        std::memcpy(base + static_cast<std::size_t>(y) * row + x0,
                    worn_.data() + static_cast<std::size_t>(y - r.y * s) * srcRow +
                        static_cast<std::size_t>(x0 - r.x * s),
                    static_cast<std::size_t>(x1 - x0) * sizeof(std::uint32_t));
}

} // namespace spux::inline v1::ui::classic
