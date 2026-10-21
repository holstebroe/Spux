#include "spux/ui/classic/ClassicStyle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "spux/params/ParamInfo.hpp"

namespace spux::inline v1::ui::classic {

namespace {

constexpr float kStartAngle = 120.f * kPi / 180.f; // 7 o'clock
constexpr float kTotalAngle = 300.f * kPi / 180.f; // to 5 o'clock
constexpr int kGripRidges = 30;

const Theme& themeOf(const Theme* t, const ClassicConfig& c) { return t ? *t : c.theme; }

// A widget whose accent is the theme's default has no color of its own.
bool hasOwnColor(const Theme& t, Argb accent) { return accent != 0 && accent != t.colors.accent; }

bool isBipolar(const params::ParamInfo* p) { return p && p->min < 0.0 && p->max > 0.0; }

// Straight-alpha blend of two ARGB colors.
Argb mixArgb(Argb a, Argb b, float t) { return ui::mix(a, b, t); }

// A line of text centered in a row of the given height starting at rowTop.
float capTopIn(float rowTop, float rowH, float cap) { return rowTop + (rowH - cap) * 0.5f; }

} // namespace

// --- LED -------------------------------------------------------------------------------------------------

void drawLed(ShadedRenderer& r, Point c, float radius, Argb color, bool lit, float level) {
    Graphics g = r.graphics();
    const float k = lit ? clamp01(level) : 0.f; // how lit
    const float s = static_cast<float>(g.getScale());
    const Color base = fromArgb(color);
    const float halo = lit ? radius * 2.4f : radius + 1.5f;
    shadeBox(g, c.x - halo - 1, c.y - halo - 1, c.x + halo + 1, c.y + halo + 1, [&](float x, float y) {
        const float dx = x - c.x, dy = y - c.y, d = std::sqrt(dx * dx + dy * dy);
        const float bezelR = radius + 1.2f;
        if (d > radius + 1.5f) {
            // The glow of a lit lamp falls off over the panel.
            const float a = 0.38f * k * (1.f - smoothstep(radius, halo, d));
            return a > 0.004f ? toArgb(base, a) : 0u;
        }
        if (d > radius - 0.2f) {
            // The bezel: a dark ring, lit on the upper left.
            const float facing = d > 1e-4f ? clamp01(-0.707f * (dx + dy) / d) : 0.f;
            const float cov = clamp01((bezelR - d) * s + 0.5f);
            return toArgb(mix(Color{0.02f, 0.02f, 0.022f}, Color{0.52f, 0.54f, 0.56f}, 0.55f * facing), cov);
        }
        // The lens: dark glass, or a hot core fading to the color at the rim.
        const float t = d / std::max(radius, 1e-3f);
        const Color dark = scale(base, 0.22f - 0.06f * t);
        Color col = mix(dark, mix(Color{1.f, 1.f, 0.95f}, base, clamp01(0.25f + 0.9f * t)), k);
        const float hl = clamp01(1.f - std::hypot(dx + 0.35f * radius, dy + 0.4f * radius) / (0.55f * radius));
        col = mix(col, Color{1.f, 1.f, 1.f}, (0.22f + 0.03f * k) * hl * hl);
        return toArgb(col, 1.f);
    });
}

// --- knob ------------------------------------------------------------------------------------------------

namespace {

struct KnobGeo {
    float cx, cy, radius;
};

KnobGeo knobGeo(const Rect& b, const ClassicConfig& c) {
    const ClassicConfig::Knob& k = c.knob;
    const float availH = static_cast<float>(b.h) - k.topRow - k.bottomRow;
    // Room for the printed scale around the dial: the knurled knob's index square reaches 14 pixels beyond
    // its radius, and half a pixel more lets a knob centered on a pixel center (odd bounds) keep its radius.
    const float room = k.shape == KnobShape::Knurled ? 15.5f : (k.ticks ? 10.f : 4.f);
    KnobGeo g;
    g.cx = static_cast<float>(b.x) + static_cast<float>(b.w) * 0.5f;
    g.cy = static_cast<float>(b.y) + k.topRow + availH * 0.5f;
    g.radius = std::max(3.f, std::min(static_cast<float>(b.w), availH) * 0.5f - room);
    return g;
}

// A dark disc, lit from the top left, with a thin light pointer and a printed scale.
void paintDiscKnob(ShadedRenderer& r, const KnobGeo& k, const KnobState& s, const ClassicConfig& c) {
    Graphics g = r.graphics();
    const float sc = static_cast<float>(g.getScale());
    const float cx = k.cx, cy = k.cy, R = k.radius;
    const bool hot = s.hot || s.active;
    const float a = kStartAngle + kTotalAngle * static_cast<float>(std::clamp(s.value, 0.0, 1.0));
    const float px = std::cos(a), py = std::sin(a);
    if (c.knob.ticks) {
        for (int i = 0; i <= 10; ++i) {
            const float t = kStartAngle + kTotalAngle * static_cast<float>(i) / 10.f;
            const float len = (i % 5 == 0) ? 5.f : 3.f;
            drawLineAA(g, cx + std::cos(t) * (R + 4.f), cy + std::sin(t) * (R + 4.f),
                       cx + std::cos(t) * (R + 4.f + len), cy + std::sin(t) * (R + 4.f + len), 1.0f,
                       (s.bipolar && i == 5) ? c.ink : c.inkDim);
        }
    }
    softShadow(g, cx - R, cy - R, cx + R, cy + R, R, 3.f, 0.6f, 1.f, 2.f);
    shadeBox(g, cx - R - 1, cy - R - 1, cx + R + 1, cy + R + 1, [&](float x, float y) {
        const float dx = x - cx, dy = y - cy, d = std::sqrt(dx * dx + dy * dy);
        const float cov = clamp01((R - d) * sc + 0.5f);
        if (cov <= 0.f)
            return 0u;
        const float lit = std::hypot(dx + 0.3f * R, dy + 0.35f * R) / (1.5f * R);
        Color col = mix(Color{0.165f, 0.175f, 0.19f}, Color{0.11f, 0.117f, 0.128f}, smoothstep(0.f, 1.f, lit));
        if (hot)
            col = scale(col, 1.2f);
        const float rim = smoothstep(R - 1.6f, R - 0.5f, d);
        const float facing = d > 1e-4f ? clamp01(-0.707f * (dx + dy) / d) : 0.f;
        col = mix(col, Color{0.56f, 0.58f, 0.60f}, rim * (0.12f + 0.55f * facing * facing));
        const float along = dx * px + dy * py, perp = std::fabs(-dx * py + dy * px);
        if (along > 0.3f * R && along < R - 2.f && perp < 0.9f)
            col = Color{0.93f, 0.95f, 0.96f};
        return toArgb(col, cov);
    });
}

// A knurled metal knob: foot ring, ridged grip with a pointer nub, polished chamfer and a spun-metal top
// with an incised pointer line, on a scale of 11 printed ticks with the index square above 12 o'clock.
void paintKnurledKnob(ShadedRenderer& r, const KnobGeo& k, const KnobState& s, const ClassicConfig& c,
                      const Theme& theme) {
    Graphics g = r.graphics();
    const float sc = static_cast<float>(g.getScale());
    const float cx = k.cx, cy = k.cy, R = k.radius;
    const float pointer = kStartAngle + kTotalAngle * static_cast<float>(std::clamp(s.value, 0.0, 1.0));

    if (c.knob.ticks) {
        for (int i = 0; i < 11; ++i) {
            const float a = kStartAngle + kTotalAngle * static_cast<float>(i) / 10.f;
            drawLineAA(g, cx + std::cos(a) * (R + 4.5f), cy + std::sin(a) * (R + 4.5f), cx + std::cos(a) * (R + 8.5f),
                       cy + std::sin(a) * (R + 8.5f), 1.4f, c.ink);
        }
        const Argb ink = c.ink;
        shadeBox(g, cx - 2.f, cy - R - 14.f, cx + 2.f, cy - R - 10.f, [ink](float, float) { return ink; });
    }

    const float rOut = R + 0.8f;   // foot ring outer edge
    const float rGrip = R - 1.6f;  // knurled grip outer edge
    const float rCham = 0.72f * R; // chamfer outer edge
    const float rTop = 0.54f * R;  // flat top

    // Soft contact shadow and a longer drop shadow towards the lower right.
    shadeBox(g, cx - R - 6, cy - R - 6, cx + R + 10, cy + R + 12, [&](float x, float y) {
        const float d0 = std::hypot(x - cx, y - cy);
        const float d1 = std::hypot(x - cx - 2.2f, y - cy - 3.2f);
        const float a =
            0.30f * (1.f - smoothstep(R - 1.f, R + 3.f, d0)) + 0.34f * (1.f - smoothstep(R - 4.f, R + 6.f, d1));
        return d0 < rOut - 0.5f ? 0u : toArgb({0.05f, 0.05f, 0.04f}, std::min(a, 0.6f));
    });

    const bool hasCap = hasOwnColor(theme, s.accent);
    const Color cap = fromArgb(s.accent);
    const Material foot{{0.15f, 0.15f, 0.16f}, 0.55f, 0.6f, 0.35f, 16.f};
    const Material grip{{0.62f, 0.63f, 0.65f}, 0.32f, 0.78f, 0.50f, 14.f};
    const Material chamfer =
        hasCap ? Material{cap, 0.38f, 0.72f, 0.40f, 30.f} : Material{{0.78f, 0.79f, 0.80f}, 0.30f, 0.75f, 0.55f, 28.f};
    const Material top =
        hasCap ? Material{cap, 0.40f, 0.68f, 0.30f, 40.f} : Material{{0.80f, 0.81f, 0.82f}, 0.34f, 0.70f, 0.40f, 22.f};
    const float px = std::cos(pointer), py = std::sin(pointer);

    shadeBox(g, cx - rOut - 1, cy - rOut - 1, cx + rOut + 1, cy + rOut + 1, [&](float x, float y) {
        const float dx = x - cx, dy = y - cy;
        const float d = std::sqrt(dx * dx + dy * dy);
        const float cov = clamp01((rOut - d) * sc + 0.5f);
        if (cov <= 0.f)
            return 0u;
        const float ux = d > 1e-4f ? dx / d : 0.f, uy = d > 1e-4f ? dy / d : 0.f;
        const float tx = -uy, ty = ux;         // tangent, clockwise on screen
        const float along = dx * px + dy * py; // along the pointer
        const float perp = dx * -py + dy * px; // across it
        const float theta = std::atan2(dy, dx);

        Color col;
        if (d > rGrip) {
            // Foot ring: steep, dark, with a bright rim on the lit side.
            const float t = (d - rGrip) / (rOut - rGrip);
            col = shadeLit(foot, tiltedNormal(ux, uy, 0.6f + 0.9f * t));
        } else if (d > rCham) {
            // Knurled grip: a steep cone with ridges that turn with the knob.
            const float t = (d - rCham) / (rGrip - rCham);
            const float phase = kGripRidges * (theta - pointer);
            Vec3 n = tiltedNormal(ux, uy, 0.95f + 0.25f * t);
            Material m = grip;
            if (along > 0.f && std::fabs(perp) < 2.2f) {
                // The pointer nub: a smooth raised rib in place of the ridges.
                const float q = perp / 2.2f;
                n = normalize({n.x * 0.6f - tx * q * 1.2f, n.y * 0.6f - ty * q * 1.2f, n.z});
                m.albedo = {0.86f, 0.87f, 0.88f};
                m.specular = 0.8f;
            } else {
                const float ridge = std::sin(phase);
                n = normalize({n.x + tx * 0.75f * ridge, n.y + ty * 0.75f * ridge, n.z});
                m.albedo = scale(m.albedo, 0.5f + 0.5f * (0.5f + 0.5f * std::cos(phase)));
            }
            col = shadeLit(m, n);
            // Dirt settles where the grip meets the top.
            col = scale(col, 0.75f + 0.25f * smoothstep(0.f, 0.25f, t));
        } else if (d > rTop) {
            // Polished chamfer.
            const float t = (d - rTop) / (rCham - rTop);
            col = shadeLit(chamfer, tiltedNormal(ux, uy, 0.42f + 0.18f * t));
        } else {
            // Top: barely domed; on the metal caps, spun-metal streaks towards and away from the light.
            Material m = top;
            if (!hasCap) {
                const float spun = std::cos(2.f * (theta - kLightAzimuth));
                const float rings = hash2(static_cast<int>(d * sc * 1.5f), 0, 11) - 0.5f;
                m.albedo = scale(m.albedo, 1.f + 0.07f * spun + 0.03f * rings);
            }
            col = shadeLit(m, tiltedNormal(ux, uy, 0.12f * d / rTop));
        }

        // Incised pointer line across the top and chamfer: dark, with its far wall catching the light.
        if (d < rCham - 0.4f && along > 0.12f * R) {
            const float w = 0.85f;
            const float core = clamp01((w - std::fabs(perp)) * sc + 0.5f);
            if (core > 0.f) {
                const Vec3 wall = tiltedNormal(perp > 0 ? -py : py, perp > 0 ? px : -px, 0.9f);
                const float lit = std::max(0.f, dot(wall, kLight));
                const Color groove = hasCap ? scale(cap, 0.25f) : Color{0.10f, 0.10f, 0.11f};
                const Color line =
                    mix(groove, Color{0.55f, 0.56f, 0.57f}, 0.35f * lit * smoothstep(0.2f, w, std::fabs(perp)));
                col = mix(col, line, core);
            }
        }
        // A little grime and wear everywhere.
        col = scale(col, 0.97f + 0.06f * (valueNoise(x * 1.7f, y * 1.7f, 21) - 0.5f));
        return toArgb(col, cov);
    });
}

} // namespace

Rect ClassicKnob::hitArea(const Rect& b) const {
    const KnobGeo g = knobGeo(b, cfg());
    const float h = g.radius + 4.f;
    return Rect{static_cast<int>(g.cx - h), static_cast<int>(g.cy - h), static_cast<int>(2 * h),
                static_cast<int>(2 * h)}
        .intersect(b);
}

void ClassicKnob::paint(IRenderer& rr, const Rect& b, const KnobState& s) const {
    ShadedRenderer* r = asShaded(rr);
    if (!r)
        return;
    const ClassicConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    const KnobGeo k = knobGeo(b, c);
    r->pushClip(b);
    if (c.knob.shape == KnobShape::Knurled)
        paintKnurledKnob(*r, k, s, c, theme);
    else
        paintDiscKnob(*r, k, s, c);

    // The label and the value, in the rows the config reserves.
    const ClassicConfig::Knob& kc = c.knob;
    const float topRowY = static_cast<float>(b.y), bottomRowY = static_cast<float>(b.bottom()) - kc.bottomRow;
    const float labelRowY = kc.labelBelow ? bottomRowY : topRowY, labelRowH = kc.labelBelow ? kc.bottomRow : kc.topRow;
    const float valueRowY = kc.labelBelow ? topRowY : bottomRowY, valueRowH = kc.labelBelow ? kc.topRow : kc.bottomRow;
    if (s.label && *s.label && labelRowH > 0.f)
        r->inkCentered(ShadedRenderer::labelFont(), s.label, k.cx, capTopIn(labelRowY, labelRowH, c.labelCap),
                       c.labelCap, c.ink, c.wear);
    if (kc.showValue && s.showValue && valueRowH > 0.f) {
        std::string text = s.editing ? std::string(s.editText) + (s.caret ? "_" : "") : std::string(s.valueText);
        if (!text.empty())
            r->inkCentered(ShadedRenderer::displayFont(), text.c_str(), k.cx,
                           capTopIn(valueRowY, valueRowH, c.valueCap), c.valueCap,
                           s.editing ? c.ink : (s.dimmed ? c.inkDim : c.valueInk));
    }
    if (s.focused) {
        const float rr2 = k.radius + 2.5f;
        r->strokeCircle({k.cx, k.cy}, rr2, withAlpha(theme.colors.focus, 0.9f), 1.0f);
    }
    r->popClip();
}

// --- slider ----------------------------------------------------------------------------------------------

namespace {

struct SliderGeo {
    float cx, cy; // the slot's center line
    float lo, hi; // travel of the cap's center along the slot: lo = value 0, hi = value 1 (screen coordinates)
    bool vertical;
};

SliderGeo sliderGeo(const Rect& b, bool vertical, const ClassicConfig& c) {
    SliderGeo g;
    g.vertical = vertical;
    g.cx = static_cast<float>(b.x) + static_cast<float>(b.w) * 0.5f;
    g.cy = static_cast<float>(b.y) + static_cast<float>(b.h) * 0.5f;
    if (vertical) {
        const float half = c.slider.capH * 0.5f + 1.5f;
        g.lo = static_cast<float>(b.bottom()) - c.knob.bottomRow - half;
        g.hi = static_cast<float>(b.y) + c.knob.topRow + half;
    } else {
        const float half = c.slider.capW * 0.5f + 1.5f;
        g.lo = static_cast<float>(b.x) + half;
        g.hi = static_cast<float>(b.right()) - half;
    }
    return g;
}

} // namespace

Rect ClassicSlider::track(const Rect& b, const SliderState& s) const {
    const SliderGeo g = sliderGeo(b, s.vertical, cfg());
    if (s.vertical)
        return {static_cast<int>(g.cx - cfg().slider.capW * 0.5f), static_cast<int>(g.hi),
                static_cast<int>(cfg().slider.capW), std::max(1, static_cast<int>(g.lo - g.hi))};
    return {static_cast<int>(g.lo), static_cast<int>(g.cy - cfg().slider.capH * 0.5f),
            std::max(1, static_cast<int>(g.hi - g.lo)), static_cast<int>(cfg().slider.capH)};
}

void ClassicSlider::paint(IRenderer& rr, const Rect& b, const SliderState& s) const {
    ShadedRenderer* r = asShaded(rr);
    if (!r)
        return;
    const ClassicConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    const SliderGeo k = sliderGeo(b, s.vertical, c);
    const bool hot = s.hot || s.active;
    const bool bipolar = isBipolar(s.param);
    const float hw = c.slider.capW * 0.5f, hh = c.slider.capH * 0.5f;
    r->pushClip(b.expand(margin()));
    Graphics g = r->graphics();

    // The slot: a dark recessed groove with its lower rim lit, and the printed scale beside it.
    const float slotW = 5.f;
    if (s.vertical)
        shadeRoundRect(
            g, k.cx - slotW / 2, k.hi - 3.f, k.cx + slotW / 2, k.lo + 3.f, slotW / 2, [&](float, float v, float d) {
                const float rim = smoothstep(-1.2f, 0.f, d);
                return toArgb(mix({0.015f, 0.015f, 0.018f}, {0.30f, 0.31f, 0.32f}, rim * (v > 0 ? 0.9f : 0.3f)), 1.f);
            });
    else
        shadeRoundRect(
            g, k.lo - 3.f, k.cy - slotW / 2, k.hi + 3.f, k.cy + slotW / 2, slotW / 2, [&](float, float v, float d) {
                const float rim = smoothstep(-1.2f, 0.f, d);
                return toArgb(mix({0.015f, 0.015f, 0.018f}, {0.30f, 0.31f, 0.32f}, rim * (v > 0 ? 0.9f : 0.3f)), 1.f);
            });
    for (int i = 0; i <= 10; ++i) {
        const float t = static_cast<float>(i) / 10.f;
        const float len = (i % 5 == 0) ? 4.f : 2.5f;
        const Argb tick = (bipolar && i == 5) ? c.ink : c.inkDim;
        if (s.vertical) {
            const float y = k.lo + (k.hi - k.lo) * t, x = k.cx - hw - 1.5f;
            drawLineAA(g, x - len, y, x, y, 0.8f, tick);
        } else {
            const float x = k.lo + (k.hi - k.lo) * t, y = k.cy + hh + 2.f;
            drawLineAA(g, x, y, x, y + len, 0.8f, tick);
        }
    }

    // The cap: a vertical gradient of its color, the edge lit along the top and left; the color is the
    // accent, or a dark gray cap when the widget has none of its own.
    const float pos = k.lo + (k.hi - k.lo) * static_cast<float>(std::clamp(s.value, 0.0, 1.0));
    const float capX = s.vertical ? k.cx : pos, capY = s.vertical ? pos : k.cy;
    const bool dark = !hasOwnColor(theme, s.accent);
    const Color capC = fromArgb(s.accent);
    softShadow(g, capX - hw, capY - hh, capX + hw, capY + hh, 3.f, 3.f, 0.6f, 1.f, 2.f);
    shadeRoundRect(g, capX - hw, capY - hh, capX + hw, capY + hh, 3.f, [&](float u, float v, float d) {
        Color col = dark ? mix(Color{0.33f, 0.345f, 0.36f}, Color{0.15f, 0.16f, 0.17f}, 0.5f + 0.5f * v)
                         : mix(scale(capC, 1.12f), scale(capC, 0.62f), 0.5f + 0.5f * v);
        if (hot)
            col = scale(col, 1.18f);
        const float lit = clamp01(0.5f - 0.5f * v + 0.35f * (u < -0.6f ? 1.f : 0.f));
        const Color edge = mix(dark ? Color{0.28f, 0.295f, 0.31f} : scale(capC, 0.55f),
                               dark ? Color{0.66f, 0.68f, 0.70f} : mix(capC, Color{1.f, 1.f, 1.f}, 0.55f), lit);
        return toArgb(mix(col, edge, smoothstep(-1.5f, -0.7f, d) * 0.8f), 1.f);
    });
    const Argb index = dark ? 0xFFE0E0DA : 0xFF151515;
    if (s.vertical)
        drawLineAA(g, capX - hw + 2.f, capY, capX + hw - 2.f, capY, 1.1f, index);
    else
        drawLineAA(g, capX, capY - hh + 2.f, capX, capY + hh - 2.f, 1.1f, index);

    // The value above and the label below, in the rows of the knob.
    if (s.vertical) {
        const ClassicConfig::Knob& kc = c.knob;
        if (kc.showValue && kc.topRow > 0.f && s.valueText && *s.valueText)
            r->inkCentered(ShadedRenderer::displayFont(), s.valueText, k.cx,
                           capTopIn(static_cast<float>(b.y), kc.topRow, c.valueCap), c.valueCap, c.valueInk);
        if (kc.bottomRow > 0.f && s.label && *s.label)
            r->inkCentered(ShadedRenderer::labelFont(), s.label, k.cx,
                           capTopIn(static_cast<float>(b.bottom()) - kc.bottomRow, kc.bottomRow, c.labelCap),
                           c.labelCap, c.ink, c.wear);
    }
    if (s.focused)
        r->strokeRoundRect(b.inset(1), 3.f, withAlpha(theme.colors.focus, 0.9f), 1.0f);
    r->popClip();
}

// --- toggle ----------------------------------------------------------------------------------------------

namespace {

// A rocker key in a black well (the bounds less 2 pixels; the key sits 1 pixel inside the well): the pressed
// half is in shadow, the other catches the light; a lamp under the lit half when on.
void paintRocker(ShadedRenderer& r, const Rect& b, const ToggleState& s, const Theme& theme) {
    Graphics g = r.graphics();
    const float wx0 = static_cast<float>(b.x) + 2.f, wy0 = static_cast<float>(b.y) + 2.f;
    const float wx1 = static_cast<float>(b.right()) - 2.f, wy1 = static_cast<float>(b.bottom()) - 2.f;
    shadeRoundRect(g, wx0, wy0, wx1, wy1, 3.f, [](float, float, float) { return 0xFF050506u; });
    const float x0 = wx0 + 3.f, y0 = wy0 + 3.f, x1 = wx1 - 3.f, y1 = wy1 - 3.f;
    const Color capC = fromArgb(s.accent ? s.accent : theme.colors.accent);
    const bool hot = s.hot || s.active;
    shadeRoundRect(g, x0, y0, x1, y1, 2.f, [&](float u, float v, float d) {
        const bool upper = v < 0.f;
        const bool raised = s.on ? upper : !upper;
        float k = raised ? 1.08f - 0.15f * std::fabs(v) : 0.62f + 0.1f * std::fabs(v);
        if (raised && (upper ? v < -0.85f : v > 0.85f))
            k = 1.25f;
        Color col = scale(capC, k);
        const float rim = smoothstep(-1.2f, 0.f, d);
        col = mix(col, scale(col, u < 0 ? 1.15f : 0.75f), rim * 0.5f);
        if (hot)
            col = scale(col, 1.06f);
        return toArgb(col, 1.f);
    });
    const float midY = 0.5f * (y0 + y1);
    drawLineAA(g, x0 + 1.f, midY, x1 - 1.f, midY, 0.8f, 0x60000000);
    if (s.on) {
        const float lx = 0.5f * (x0 + x1), ly = y1 - 5.f;
        shadeBox(g, lx - 5, ly - 5, lx + 5, ly + 5, [&](float x, float y) {
            const float dd = std::hypot(x - lx, y - ly);
            return toArgb({1.0f, 0.22f, 0.12f}, clamp01(1.25f - dd / 3.f));
        });
    }
}

// A lever switch in an escutcheon plate: on is the lever down, off up.
void paintLever(ShadedRenderer& r, const Rect& b, const ToggleState& s) {
    Graphics g = r.graphics();
    const float sc = static_cast<float>(g.getScale());
    const float cx = static_cast<float>(b.x) + static_cast<float>(b.w) * 0.5f;
    const float cy = static_cast<float>(b.y) + static_cast<float>(b.h) * 0.5f;
    // Shadow under the escutcheon plate.
    shadeBox(g, cx - 12, cy - 25, cx + 14, cy + 28, [&](float x, float y) {
        const float d = sdRoundRect(x - cx - 1.5f, y - cy - 2.2f, 8.5f, 21.f, 3.f);
        const float a = 0.38f * (1.f - smoothstep(-2.f, 4.f, d));
        return a > 0.f ? toArgb({0.05f, 0.05f, 0.04f}, a) : 0u;
    });
    const Material plate{{0.74f, 0.75f, 0.76f}, 0.35f, 0.70f, 0.45f, 20.f};
    const Material slot{{0.07f, 0.07f, 0.08f}, 0.5f, 0.7f, 0.2f, 10.f};
    const Material lever{{0.86f, 0.87f, 0.88f}, 0.30f, 0.72f, 0.55f, 26.f};
    const float leverY = cy + (s.on ? 9.f : -9.f);
    shadeBox(g, cx - 10, cy - 23, cx + 10, cy + 23, [&](float x, float y) {
        const float lx = x - cx, ly = y - cy;
        const float dPlate = sdRoundRect(lx, ly, 8.5f, 21.f, 2.5f);
        const float cov = clamp01(-dPlate * sc + 0.5f);
        if (cov <= 0.f)
            return 0u;
        float ux, uy;
        Color col;
        const float dSlot = sdRoundRect(lx, ly, 4.f, 17.f, 2.f);
        if (dSlot > 0.f) {
            // Plate with a bevelled edge.
            sdRoundRectDir(lx, ly, 8.5f, 21.f, 2.5f, ux, uy);
            const float bevel = smoothstep(-1.6f, 0.f, dPlate);
            col = shadeLit(plate, tiltedNormal(ux, uy, 0.9f * bevel));
            col = scale(col, 1.f + 0.03f * (valueNoise(x * 0.2f, y * 6.f, 31) - 0.5f));
            // The slot's lip.
            col = scale(col, 0.6f + 0.4f * smoothstep(0.f, 1.f, dSlot));
        } else {
            // Recess: walls facing the light are lit, the rest in shadow.
            sdRoundRectDir(lx, ly, 4.f, 17.f, 2.f, ux, uy);
            const float wall = smoothstep(-1.8f, 0.f, dSlot);
            col = shadeLit(slot, tiltedNormal(-ux, -uy, 1.1f * wall));
        }
        // Lever shadow in the slot, then the lever itself.
        const float dShadow = sdRoundRect(lx - 1.2f, y - leverY - 1.8f, 6.5f, 6.5f, 2.f);
        col = scale(col, 1.f - 0.55f * (1.f - smoothstep(-1.5f, 2.5f, dShadow)));
        const float dLever = sdRoundRect(lx, y - leverY, 6.5f, 6.5f, 2.f);
        const float lcov = clamp01(-dLever * sc + 0.5f);
        if (lcov > 0.f) {
            sdRoundRectDir(lx, y - leverY, 6.5f, 6.5f, 2.f, ux, uy);
            const float bevel = smoothstep(-2.2f, 0.f, dLever);
            Vec3 n = tiltedNormal(ux, uy, 0.8f * bevel);
            // Three grip grooves across the lever's face.
            const float gy = y - leverY;
            if (bevel <= 0.f && std::fabs(gy) < 4.5f) {
                const float ridge = std::sin(gy * kPi / 1.5f);
                n = normalize({n.x, n.y + 0.5f * ridge, n.z});
            }
            col = mix(col, shadeLit(lever, n), lcov);
        }
        return toArgb(col, cov);
    });
}

} // namespace

void ClassicToggle::paint(IRenderer& rr, const Rect& b, const ToggleState& s) const {
    ShadedRenderer* r = asShaded(rr);
    if (!r)
        return;
    const ClassicConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    r->pushClip(b);
    if (c.toggle == ToggleShape::Lever) {
        paintLever(*r, b, s);
        // The label above the plate, when the bounds are taller than it.
        const float room = (static_cast<float>(b.h) - 46.f) * 0.5f;
        if (s.label && *s.label && room >= c.labelCap + 3.f)
            r->inkCentered(ShadedRenderer::labelFont(), s.label,
                           static_cast<float>(b.x) + static_cast<float>(b.w) * 0.5f, static_cast<float>(b.y) + 2.f,
                           c.labelCap, c.ink, c.wear);
    } else {
        paintRocker(*r, b, s, theme);
    }
    if (s.focused)
        r->strokeRoundRect(b.inset(1), 3.f, withAlpha(theme.colors.focus, 0.9f), 1.0f);
    r->popClip();
}

// --- selector --------------------------------------------------------------------------------------------

namespace {

// The label of step i: the page's, the parameter's enum label, or the number.
std::string stepLabel(const SelectorState& s, int i) {
    if (s.labels && s.labels[i])
        return s.labels[i];
    if (s.param && params::enumCount(*s.param) > 0)
        return std::string(params::enumLabel(*s.param, static_cast<std::size_t>(i)));
    char buf[24];
    std::snprintf(buf, sizeof buf, "%d", (s.param ? static_cast<int>(s.param->min) : 0) + i);
    return buf;
}

// A dark rounded button face with a light edge (bright when selected), lit from above.
void paintButtonFace(Graphics& g, float x0, float y0, float x1, float y1, float radius, bool selected, bool hot,
                     bool pressed, const ClassicConfig& c) {
    const Color edge = selected ? c.button.edgeOn : c.button.edge;
    const float bw = selected ? 1.5f : 1.f;
    shadeRoundRect(g, x0, y0, x1, y1, radius, [&](float, float v, float d) {
        Color fill = mix(c.button.top, c.button.bottom, 0.5f + 0.5f * v);
        if (selected)
            fill = scale(fill, 1.1f);
        if (pressed)
            fill = scale(fill, 0.8f);
        if (hot)
            fill = scale(fill, 1.15f);
        return toArgb(mix(fill, edge, smoothstep(-bw - 0.7f, -bw, d)), 1.f);
    });
}

} // namespace

Rect ClassicSelector::cell(const Rect& b, const SelectorState& s, int i) const {
    const int n = s.count > 0 ? s.count : 1;
    if (s.vertical) {
        // The lever's positions sit between 4 pixels of margin at the ends.
        const float step = (static_cast<float>(b.h) - 8.f) / static_cast<float>(n);
        const int y0 = b.y + 4 + static_cast<int>(step * static_cast<float>(i));
        const int y1 = b.y + 4 + static_cast<int>(step * static_cast<float>(i + 1));
        return {b.x, y0, b.w, y1 - y0};
    }
    return SelectorPainter::cell(b, s, i);
}

void ClassicSelector::paint(IRenderer& rr, const Rect& b, const SelectorState& s) const {
    ShadedRenderer* r = asShaded(rr);
    if (!r)
        return;
    const ClassicConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    r->pushClip(b);
    Graphics g = r->graphics();
    const int n = std::max(0, s.count);
    if (s.vertical) {
        // A lever in a slot at the left, the step names to its right; the selected position holds the lever.
        const float slotX = static_cast<float>(b.x) + 7.f;
        shadeRoundRect(g, slotX - 2.5f, static_cast<float>(b.y) + 2.f, slotX + 2.5f,
                       static_cast<float>(b.bottom()) - 2.f, 2.5f, [&](float, float v, float d) {
                           const float rim = smoothstep(-1.2f, 0.f, d);
                           return toArgb(
                               mix({0.015f, 0.015f, 0.018f}, {0.30f, 0.31f, 0.32f}, rim * (v > 0 ? 0.9f : 0.3f)), 1.f);
                       });
        const bool dark = !hasOwnColor(theme, s.accent);
        const Color capC = dark ? Color{0.78f, 0.79f, 0.80f} : fromArgb(s.accent);
        const float step = (static_cast<float>(b.h) - 8.f) / static_cast<float>(std::max(1, n));
        for (int i = 0; i < n; ++i) {
            const float y = static_cast<float>(b.y) + 4.f + step * (static_cast<float>(i) + 0.5f);
            const bool sel = i == s.index;
            const Argb tint = sel ? c.inkOn : (i == s.hotIndex ? c.ink : c.inkDim);
            if (s.icons && s.icons[i]) {
                s.icons[i](*r, {static_cast<int>(slotX + 9.f), static_cast<int>(y - 4.f), 16, 8}, tint, 1.2f);
            } else {
                const std::string label = stepLabel(s, i);
                r->ink(ShadedRenderer::labelFont(), label.c_str(), slotX + 9.f, y - 2.6f, 5.2f, tint);
            }
            drawLineAA(g, slotX + 3.5f, y, slotX + 6.5f, y, 0.8f, c.inkDim);
            if (sel) {
                const float hw = 5.f, hh = 4.f;
                softShadow(g, slotX - hw, y - hh, slotX + hw, y + hh, 1.5f, 2.5f, 0.5f, 1.f, 1.5f);
                shadeRoundRect(g, slotX - hw, y - hh, slotX + hw, y + hh, 1.5f,
                               [&](float, float v, float) { return toArgb(scale(capC, 1.05f - 0.3f * v), 1.f); });
            }
        }
    } else {
        for (int i = 0; i < n; ++i) {
            const Rect cl = cell(b, s, i).inset(1);
            const bool sel = i == s.index;
            paintButtonFace(g, static_cast<float>(cl.x), static_cast<float>(cl.y), static_cast<float>(cl.right()),
                            static_cast<float>(cl.bottom()), c.button.radius * 0.75f, sel, i == s.hotIndex, false, c);
            const Argb tint = sel ? c.inkOn : c.ink;
            if (s.icons && s.icons[i]) {
                const int ih = std::max(4, std::min(14, cl.h - 8));
                const int iw = std::min(ih * 2, std::max(4, cl.w - 8));
                s.icons[i](*r, {cl.x + (cl.w - iw) / 2, cl.y + (cl.h - ih) / 2, iw, ih}, tint, 1.4f);
            } else {
                const std::string label = stepLabel(s, i);
                r->inkCentered(ShadedRenderer::labelFont(), label.c_str(),
                               static_cast<float>(cl.x) + static_cast<float>(cl.w) * 0.5f,
                               capTopIn(static_cast<float>(cl.y), static_cast<float>(cl.h), 5.f), 5.f, tint, c.wear);
            }
        }
    }
    if (s.focused)
        r->strokeRoundRect(b.inset(1), 3.f, withAlpha(theme.colors.focus, 0.9f), 1.0f);
    r->popClip();
}

// --- button ----------------------------------------------------------------------------------------------

void ClassicButton::paint(IRenderer& rr, const Rect& b, const ButtonState& s) const {
    ShadedRenderer* r = asShaded(rr);
    if (!r)
        return;
    const ClassicConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    const float x0 = static_cast<float>(b.x), y0 = static_cast<float>(b.y);
    const float x1 = static_cast<float>(b.right()), y1 = static_cast<float>(b.bottom());
    const float cx = 0.5f * (x0 + x1), cy = 0.5f * (y0 + y1);
    const bool hot = s.hot;
    r->pushClip(b.expand(margin()));
    Graphics g = r->graphics();

    switch (s.variant) {
    case kButtonCap: {
        // A colored cap, dim when off, lit when on, with a one- or two-line legend.
        const Color base = fromArgb(s.accent ? s.accent : theme.colors.accent);
        softShadow(g, x0, y0, x1, y1, 2.f, 3.f, 0.5f, 1.f, 2.f);
        shadeRoundRect(g, x0, y0, x1, y1, 2.f, [&](float u, float v, float d) {
            Color col = scale(base, s.on ? 1.05f : 0.52f);
            col = scale(col, 1.02f - 0.12f * v);
            if (s.on)
                col = mix(col, {1.f, 1.f, 0.95f}, 0.25f * (1.f - std::fabs(u)) * (1.f - std::fabs(v)));
            const float rim = smoothstep(-1.3f, 0.f, d);
            col = mix(col, scale(col, v < 0 ? 1.25f : 0.6f), rim * 0.5f);
            if (hot)
                col = scale(col, 1.08f);
            return toArgb(col, 1.f);
        });
        const Argb legend = s.on ? 0xF0181818 : 0xC0101010;
        const char* label = s.label ? s.label : "";
        const char* nl = std::strchr(label, '\n');
        const float h = y1 - y0;
        if (nl) {
            const std::string top(label, static_cast<std::size_t>(nl - label));
            r->inkCentered(ShadedRenderer::labelFont(), top.c_str(), cx, y0 + h * 0.31f, 4.4f, legend);
            r->inkCentered(ShadedRenderer::labelFont(), nl + 1, cx, y0 + h * 0.62f, 5.5f, legend);
        } else {
            r->inkCentered(ShadedRenderer::labelFont(), label, cx, capTopIn(y0, h, 5.5f), 5.5f, legend);
        }
        break;
    }
    default: {
        const bool arrow = s.variant == kButtonArrowLeft || s.variant == kButtonArrowRight;
        softShadow(g, x0, y0, x1, y1, c.button.radius, 3.f, 0.5f, 1.f, 2.f);
        paintButtonFace(g, x0, y0, x1, y1, c.button.radius, s.on, hot, s.active, c);
        if (arrow) {
            const float k = s.variant == kButtonArrowLeft ? -1.f : 1.f;
            drawLineAA(g, cx - 3.f * k, cy - 5.f, cx + 3.f * k, cy, 1.6f, 0xFFE8E8E2);
            drawLineAA(g, cx + 3.f * k, cy, cx - 3.f * k, cy + 5.f, 1.6f, 0xFFE8E8E2);
        } else {
            const Argb ink = s.on ? c.inkOn : c.ink;
            const bool text = s.label && *s.label;
            float textX = cx;
            if (s.icon) {
                // the icon alone is centered; beside a label it takes the left end
                const int ih = std::max(4, std::min(14, b.h - 10));
                const int iw = std::min(ih * 2, std::max(4, b.w - 10));
                const float ix = text ? x0 + 6.f : cx - static_cast<float>(iw) * 0.5f;
                s.icon(*r, {static_cast<int>(ix), static_cast<int>(cy - static_cast<float>(ih) * 0.5f), iw, ih}, ink,
                       1.4f);
                textX = x0 + 6.f + static_cast<float>(iw) + (x1 - x0 - 6.f - static_cast<float>(iw)) * 0.5f;
            }
            if (text)
                r->inkCentered(ShadedRenderer::labelFont(), s.label, textX, capTopIn(y0, y1 - y0, 5.f), 5.f, ink,
                               c.wear);
        }
        break;
    }
    }
    if (s.focused)
        r->strokeRoundRect(b.inset(1), c.button.radius, withAlpha(theme.colors.focus, 0.9f), 1.0f);
    r->popClip();
}

// --- panel -----------------------------------------------------------------------------------------------

void ClassicPanel::paint(IRenderer& rr, const Rect& b, const PanelState& s) const {
    ShadedRenderer* r = asShaded(rr);
    if (!r)
        return;
    const ClassicConfig& c = cfg();
    r->pushClip(b);
    const bool titled = s.title && *s.title;
    if (!titled) {
        // The surface.
        if (c.surface == PanelSurface::Worn) {
            r->wornPanel(b, c.trims);
        } else {
            Graphics g = r->graphics();
            const float W = static_cast<float>(b.w), H = static_cast<float>(b.h);
            const float ox = static_cast<float>(b.x), oy = static_cast<float>(b.y);
            const Color light = c.surfaceLight, dark = c.surfaceDark;
            shadeBox(g, ox, oy, ox + W, oy + H, [&](float x, float y) {
                const float lx = x - ox, ly = y - oy;
                const float grain = 0.008f * (hash2(static_cast<int>(lx * 2), static_cast<int>(ly * 0.5f), 7) - 0.5f);
                const float t = clamp01(0.45f * lx / W + 0.8f * ly / H);
                const Color col = mix(light, dark, t);
                return toArgb({col.r + grain, col.g + grain, col.b + grain}, 1.f);
            });
            r->fillRect({b.x, b.y, b.w, std::min(2, b.h)}, 0xFF2C2D30);
            r->fillRect({b.x, b.bottom() - std::min(2, b.h), b.w, std::min(2, b.h)}, 0xFF08090A);
        }
    } else {
        // A frame with the title cut into its top line.
        Graphics g = r->graphics();
        const float x0 = static_cast<float>(b.x) + 0.5f, y0 = static_cast<float>(b.y) + 6.5f;
        const float x1 = static_cast<float>(b.right()) - 0.5f, y1 = static_cast<float>(b.bottom()) - 0.5f;
        const float tw = classic::textWidth(ShadedRenderer::labelFont(), s.title, c.titleCap) + 8.f;
        const float cx = 0.5f * (x0 + x1);
        drawLineAA(g, x0, y0, cx - tw / 2, y0, 1.f, c.frameInk);
        drawLineAA(g, cx + tw / 2, y0, x1, y0, 1.f, c.frameInk);
        drawLineAA(g, x0, y0, x0, y1, 1.f, c.frameInk);
        drawLineAA(g, x1, y0, x1, y1, 1.f, c.frameInk);
        drawLineAA(g, x0, y1, x1, y1, 1.f, c.frameInk);
        r->inkCentered(ShadedRenderer::labelFont(), s.title, cx, static_cast<float>(b.y) + 3.f, c.titleCap, c.ink,
                       c.wear);
    }
    r->popClip();
}

// --- lamp ------------------------------------------------------------------------------------------------

void ClassicLed::paint(IRenderer& rr, const Rect& b, const LedState& s) const {
    ShadedRenderer* r = asShaded(rr);
    if (!r)
        return;
    const ClassicConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    const float y0 = static_cast<float>(b.y), x0 = static_cast<float>(b.x);
    const float h = static_cast<float>(b.h), cy = y0 + h * 0.5f;
    const float radius = std::clamp(h * 0.5f - 2.f, 2.5f, 4.5f);
    r->pushClip(b.expand(margin()));
    drawLed(*r, {x0 + radius + 3.f, cy}, radius, s.accent ? s.accent : theme.colors.accent, s.on, s.level);
    if (s.label && *s.label)
        r->ink(ShadedRenderer::labelFont(), s.label, x0 + 2.f * radius + 9.f, capTopIn(y0, h, c.labelCap), c.labelCap,
               s.on ? c.ink : c.inkDim, c.wear);
    r->popClip();
}

// --- keyboard --------------------------------------------------------------------------------------------

void ClassicKeyboard::paint(IRenderer& rr, const Rect& b, const KeyboardState& s) const {
    ShadedRenderer* r = asShaded(rr);
    if (!r || b.empty())
        return;
    const ClassicConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    const keyboard::Layout lay = layout();
    const Color accent = fromArgb(s.accent ? s.accent : theme.colors.accent);
    const float top = static_cast<float>(b.y), h = static_cast<float>(b.h);
    r->pushClip(b.expand(margin()));
    Graphics g = r->graphics();
    // The felt strip behind the keys, then the white keys, then the black ones over them.
    shadeBox(g, static_cast<float>(b.x), top, static_cast<float>(b.right()), top + 3.f,
             [felt = c.keyboardFelt](float, float) { return felt; });
    for (int k = s.first; k <= s.last; ++k) {
        if (keyboard::isBlack(k))
            continue;
        const keyboard::KeyRect kr = keyboard::keyRect(b, s.first, s.last, k, lay);
        const bool down = s.down && s.down[k], sounding = s.lit && s.lit[k];
        const bool hot = s.hotKey == k && !down;
        const float x0 = kr.x0 + 0.5f, x1 = kr.x1 - 0.5f;
        shadeBox(g, x0, top + 3.f, x1, top + h, [&](float xx, float y) {
            const float v = (y - top) / h;
            Color col = down ? Color{0.80f, 0.76f, 0.62f} : Color{0.94f, 0.92f, 0.86f};
            if (sounding)
                col = mix(col, accent, 0.45f);
            col = scale(col, 0.88f + 0.12f * v);
            if (hot)
                col = scale(col, 1.04f);
            if (xx - x0 < 1.f || x1 - xx < 1.f)
                col = scale(col, 0.7f);
            if (y > top + h - 4.f)
                col = scale(col, down ? 0.7f : 0.82f);
            return toArgb(col, 1.f);
        });
    }
    for (int k = s.first; k <= s.last; ++k) {
        if (!keyboard::isBlack(k))
            continue;
        const keyboard::KeyRect kr = keyboard::keyRect(b, s.first, s.last, k, lay);
        const bool down = s.down && s.down[k], sounding = s.lit && s.lit[k];
        const float cx = 0.5f * (kr.x0 + kr.x1), bw = kr.x1 - kr.x0;
        softShadow(g, kr.x0, top + 3.f, kr.x1, kr.y1, 1.f, 2.5f, 0.5f, 1.f, 1.5f);
        shadeBox(g, kr.x0, top + 3.f, kr.x1, kr.y1, [&](float xx, float y) {
            const float v = (y - top) / (kr.y1 - top);
            Color col = down ? Color{0.28f, 0.22f, 0.12f} : Color{0.06f, 0.06f, 0.065f};
            if (sounding)
                col = mix(col, scale(accent, 0.6f), 0.6f);
            col = scale(col, 1.f + 0.6f * (1.f - std::fabs((xx - cx) / (bw * 0.5f))) * (v > 0.85f ? 2.f : 0.4f));
            return toArgb(col, 1.f);
        });
    }
    r->popClip();
}

// --- display widgets -------------------------------------------------------------------------------------

namespace {

// The recessed display of a dropdown, a stepper and a text field.
void paintDisplay(Graphics& g, const Rect& b, bool hot, Argb edgeOverride, const ClassicConfig& c) {
    const Color edge = edgeOverride ? fromArgb(edgeOverride) : c.display.edge;
    shadeRoundRect(g, static_cast<float>(b.x), static_cast<float>(b.y), static_cast<float>(b.right()),
                   static_cast<float>(b.bottom()), c.display.radius, [&](float, float v, float d) {
                       Color col = mix(c.display.top, c.display.bottom, 0.5f + 0.5f * v);
                       if (hot)
                           col = scale(col, 1.4f);
                       return toArgb(mix(col, edge, smoothstep(-1.5f, -0.5f, d)), 1.f);
                   });
}

// Text in the display font, shrunk to fit `room` and centered on `cx` (or starting at x with align left).
void displayText(ShadedRenderer& r, const char* text, float x, float room, bool centered, const Rect& b, Argb ink,
                 float cap) {
    while (cap > 4.5f && ShadedRenderer::inkWidth(ShadedRenderer::displayFont(), text, cap) > room)
        cap -= 0.25f;
    const float capTop = capTopIn(static_cast<float>(b.y), static_cast<float>(b.h), cap);
    if (centered)
        r.inkCentered(ShadedRenderer::displayFont(), text, x, capTop, cap, ink);
    else
        r.ink(ShadedRenderer::displayFont(), text, x, capTop, cap, ink);
}

} // namespace

void ClassicDropdown::paint(IRenderer& rr, const Rect& b, const DropdownState& s) const {
    ShadedRenderer* r = asShaded(rr);
    if (!r)
        return;
    const ClassicConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    r->pushClip(b);
    Graphics g = r->graphics();
    paintDisplay(g, b, s.hot && !s.open, s.open ? theme.colors.focus : 0u, c);
    float tx = static_cast<float>(b.x) + 8.f;
    if (s.label && *s.label) {
        r->ink(ShadedRenderer::labelFont(), s.label, tx,
               capTopIn(static_cast<float>(b.y), static_cast<float>(b.h), 5.f), 5.f, c.inkDim);
        tx += ShadedRenderer::inkWidth(ShadedRenderer::labelFont(), s.label, 5.f) + 8.f;
    }
    displayText(*r, s.valueText ? s.valueText : "", tx, static_cast<float>(b.right()) - tx - 20.f, false, b,
                c.display.ink, c.valueCap);
    const float cx = static_cast<float>(b.right()) - 11.f,
                cy = static_cast<float>(b.y) + static_cast<float>(b.h) * 0.5f;
    drawLineAA(g, cx - 3.5f, cy - 1.5f, cx, cy + 2.f, 1.4f, c.inkDim);
    drawLineAA(g, cx, cy + 2.f, cx + 3.5f, cy - 1.5f, 1.4f, c.inkDim);
    if (s.focused)
        r->strokeRoundRect(b.inset(1), c.display.radius, withAlpha(theme.colors.focus, 0.9f), 1.0f);
    r->popClip();
}

void ClassicStepper::paint(IRenderer& rr, const Rect& b, const StepperState& s) const {
    ShadedRenderer* r = asShaded(rr);
    if (!r)
        return;
    const ClassicConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    r->pushClip(b);
    Graphics g = r->graphics();
    paintDisplay(g, b, false, 0u, c);
    for (int k = 0; k < 2; ++k) {
        const Rect part = (k == 0 ? minusPart(b) : plusPart(b)).inset(1);
        const bool hot = s.hotPart == (k == 0 ? -1 : 1);
        paintButtonFace(g, static_cast<float>(part.x), static_cast<float>(part.y), static_cast<float>(part.right()),
                        static_cast<float>(part.bottom()), c.button.radius * 0.75f, false, hot, false, c);
        const Point mid = part.center();
        drawLineAA(g, mid.x - 3.5f, mid.y, mid.x + 3.5f, mid.y, 1.5f, 0xFFE8E8E2);
        if (k == 1)
            drawLineAA(g, mid.x, mid.y - 3.5f, mid.x, mid.y + 3.5f, 1.5f, 0xFFE8E8E2);
    }
    std::string text = s.valueText ? s.valueText : "";
    if (s.label && *s.label)
        text = std::string(s.label) + " " + text;
    const Rect lo = minusPart(b), hi = plusPart(b);
    displayText(*r, text.c_str(), static_cast<float>(lo.right()) + static_cast<float>(hi.x - lo.right()) * 0.5f,
                static_cast<float>(hi.x - lo.right()) - 6.f, true, b, c.display.ink, c.valueCap);
    if (s.focused)
        r->strokeRoundRect(b.inset(1), c.display.radius, withAlpha(theme.colors.focus, 0.9f), 1.0f);
    r->popClip();
}

void ClassicTextField::paint(IRenderer& rr, const Rect& b, const TextFieldState& s) const {
    ShadedRenderer* r = asShaded(rr);
    if (!r)
        return;
    const ClassicConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    r->pushClip(b);
    Graphics g = r->graphics();
    paintDisplay(g, b, false, theme.colors.focus, c);
    const std::string shown = std::string(s.text ? s.text : "") + (s.caret ? "_" : " ");
    displayText(*r, shown.c_str(), static_cast<float>(b.x) + 8.f, static_cast<float>(b.w) - 16.f, false, b,
                c.display.ink, c.valueCap);
    r->popClip();
}

// --- popup and tooltip -----------------------------------------------------------------------------------

namespace {

constexpr int kPopupPad = 4, kSeparatorH = 7;

// A panel with a soft shadow, a flat fill and a light edge.
void paintFloating(ShadedRenderer& r, const Rect& b, Argb fill, const ClassicConfig& c) {
    Graphics g = r.graphics();
    softShadow(g, static_cast<float>(b.x), static_cast<float>(b.y), static_cast<float>(b.right()),
               static_cast<float>(b.bottom()), 3.f, 6.f, 0.7f, 1.f, 2.f);
    shadeRoundRect(g, static_cast<float>(b.x), static_cast<float>(b.y), static_cast<float>(b.right()),
                   static_cast<float>(b.bottom()), 3.f, [&](float, float, float d) {
                       return mixArgb(fill, toArgb(c.display.edge, 1.f), smoothstep(-1.5f, -0.5f, d));
                   });
}

} // namespace

Rect ClassicPopup::layout(IRenderer& r, const PopupState& s, int viewW, int viewH, std::vector<Rect>& rects) const {
    const Theme::Metrics& m = s.theme->metrics;
    const std::vector<PopupItem>& items = *s.items;
    const int n = static_cast<int>(items.size());
    const int cols = std::max(1, s.columns);
    const int rows = std::max(1, (n + cols - 1) / cols);
    float maxW = static_cast<float>(s.anchor.w);
    for (const PopupItem& it : items)
        if (!it.separator)
            maxW = std::max(maxW, r.textWidthPx(it.label.c_str(), m.popupPx) + 28.f);
    const int itemW = static_cast<int>(std::ceil(maxW));
    // column-major, so a long list reads down its columns
    std::vector<int> colY(static_cast<std::size_t>(cols), 0);
    rects.assign(static_cast<std::size_t>(n), Rect{});
    for (int i = 0; i < n; ++i) {
        const int col = i / rows;
        const int h = items[static_cast<std::size_t>(i)].separator ? kSeparatorH : m.popupRow;
        rects[static_cast<std::size_t>(i)] = {kPopupPad + col * itemW, kPopupPad + colY[static_cast<std::size_t>(col)],
                                              itemW, h};
        colY[static_cast<std::size_t>(col)] += h;
    }
    const int w = itemW * cols + kPopupPad * 2, h = *std::max_element(colY.begin(), colY.end()) + kPopupPad * 2;
    int x = s.anchor.x, y = s.anchor.empty() ? s.anchor.y : s.anchor.bottom() + 3;
    if (y + h > viewH - 4)
        y = std::max(4, s.anchor.y - h - (s.anchor.empty() ? 0 : 3));
    if (x + w > viewW - 4)
        x = std::max(4, viewW - 4 - w);
    for (Rect& it : rects) {
        it.x += x;
        it.y += y;
    }
    return {x, y, w, h};
}

void ClassicPopup::paint(IRenderer& rr, const Rect& b, const PopupState& s) const {
    ShadedRenderer* r = asShaded(rr);
    if (!r)
        return;
    const ClassicConfig& c = cfg();
    const Theme& t = themeOf(s.theme, c);
    r->pushClip(b.expand(margin()));
    paintFloating(*r, b, t.colors.popup, c);
    const float px = t.metrics.popupPx, cap = px * 0.7f;
    const std::vector<PopupItem>& items = *s.items;
    for (std::size_t i = 0; i < items.size() && s.itemRects && i < s.itemRects->size(); ++i) {
        const Rect& cell = (*s.itemRects)[i];
        const PopupItem& it = items[i];
        if (it.separator) {
            r->fillRect({cell.x + 6, cell.y + cell.h / 2, cell.w - 12, 1}, t.colors.hairline);
            continue;
        }
        const bool hot = static_cast<int>(i) == s.hot && it.enabled;
        if (hot)
            r->fillRoundRect(cell.inset(1), 2.f, withAlpha(t.colors.accent, 0.35f));
        if (static_cast<int>(i) == s.selected)
            r->fillCircle(
                {static_cast<float>(cell.x) + 9.f, static_cast<float>(cell.y) + static_cast<float>(cell.h) * 0.5f},
                2.5f, t.colors.focus);
        r->pushClip(cell);
        r->ink(ShadedRenderer::labelFont(), it.label.c_str(), static_cast<float>(cell.x) + 18.f,
               capTopIn(static_cast<float>(cell.y), static_cast<float>(cell.h), cap), cap,
               !it.enabled ? t.colors.textFaint : (hot ? t.colors.text : t.colors.textDim));
        r->popClip();
    }
    r->popClip();
}

Rect ClassicTooltip::layout(IRenderer& r, const TooltipState& s, int viewW, int viewH) const {
    const bool body = s.text && *s.text;
    const float w = std::max(ShadedRenderer::inkWidth(ShadedRenderer::labelFont(), s.title ? s.title : "", 6.5f),
                             body ? ShadedRenderer::inkWidth(ShadedRenderer::labelFont(), s.text, 5.5f) : 0.f) +
                    16.f;
    const int iw = std::min(static_cast<int>(std::ceil(w)), std::max(0, viewW - 8)), ih = body ? 34 : 22;
    int x = static_cast<int>(s.pointer.x) + 12, y = static_cast<int>(s.pointer.y) + 16;
    if (x + iw > viewW - 4)
        x = std::max(4, viewW - 4 - iw);
    if (y + ih > viewH - 4)
        y = std::max(4, static_cast<int>(s.pointer.y) - ih - 12);
    (void)r;
    return {x, y, iw, ih};
}

void ClassicTooltip::paint(IRenderer& rr, const Rect& b, const TooltipState& s) const {
    ShadedRenderer* r = asShaded(rr);
    if (!r)
        return;
    const ClassicConfig& c = cfg();
    const Theme& t = themeOf(s.theme, c);
    r->pushClip(b.expand(margin()));
    paintFloating(*r, b, t.colors.tooltip, c);
    r->pushClip(b.inset(3, 0));
    const bool body = s.text && *s.text;
    r->ink(ShadedRenderer::labelFont(), s.title ? s.title : "", static_cast<float>(b.x) + 8.f,
           static_cast<float>(b.y) + (body ? 7.f : 8.f), 6.5f, t.colors.text);
    if (body)
        r->ink(ShadedRenderer::labelFont(), s.text, static_cast<float>(b.x) + 8.f, static_cast<float>(b.y) + 21.f, 5.5f,
               t.colors.textDim);
    r->popClip();
    r->popClip();
}

} // namespace spux::inline v1::ui::classic
