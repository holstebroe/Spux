#include "spux/ui/modern/ModernStyle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "ModernInternal.hpp"
#include "spux/params/ParamInfo.hpp"

namespace spux::inline v1::ui::modern {

namespace {

void focusRing(IRenderer& r, const ModernConfig& c, const Theme& t, const Rect& area, float radius, Color accent) {
    const int g = t.metrics.focusRing;
    r.strokeRoundRect(area.expand(Margin::all(g)), radius + f(g), withAlpha(accent, 0.95f), c.shape.focusStroke);
}

Margin shadowMargin(const ModernConfig& c) { return c.shape.panelShadow ? Margin{23, 19, 23, 27} : Margin{}; }

// --- knob geometry, shared by the painter and the questions the interaction asks ----------------------

struct KnobGeo {
    float cx, cy, radius, ringRadius;
    Rect dial;
};

// The dial does not depend on the knob's size class: a large knob has larger text inside the same dial
// (the interaction asks for hitArea() without a state), so the text sizes reserved here are the defaults.
KnobGeo knobGeo(const Rect& b, const Theme::Metrics& m, const ModernConfig& c) {
    KnobGeo g;
    g.cx = f(b.x) + f(b.w) * 0.5f;
    const float bodyTop = f(b.y) + m.labelPx + 4.0f, bodyBottom = f(b.bottom()) - m.valuePx - 9.0f;
    g.radius = std::max(4.0f, std::min(f(b.w) * 0.5f - 12.0f, (bodyBottom - bodyTop) * 0.5f - 4.0f));
    g.cy = bodyTop + (bodyBottom - bodyTop) * 0.5f;
    g.ringRadius = g.radius + c.knob.modRingGap;
    g.dial = {static_cast<int>(g.cx - g.radius - 8), static_cast<int>(g.cy - g.radius - 8),
              static_cast<int>(2 * g.radius + 16), static_cast<int>(2 * g.radius + 16)};
    return g;
}

float knobAngle(const ModernConfig& c, double norm) {
    return c.knob.startAngle + c.knob.sweep * static_cast<float>(std::clamp(norm, 0.0, 1.0));
}

Point onCircle(float cx, float cy, float angle, float radius) {
    return {cx + std::cos(angle) * radius, cy + std::sin(angle) * radius};
}

} // namespace

std::string fitText(const IRenderer& r, const std::string& s, float px, float maxWidth, FontWeight weight) {
    if (r.textWidthPx(s.c_str(), px, weight) <= maxWidth)
        return s;
    std::string t = s;
    while (!t.empty() && r.textWidthPx((t + "\xE2\x80\xA6").c_str(), px, weight) > maxWidth) {
        std::size_t i = t.size() - 1; // drop one UTF-8 character
        while (i > 0 && (static_cast<unsigned char>(t[i]) & 0xC0) == 0x80)
            --i;
        t.erase(i);
    }
    return t.empty() ? t : t + "\xE2\x80\xA6";
}

// --- knob ----------------------------------------------------------------------------------------------

Rect ModernKnob::hitArea(const Rect& b) const { return knobGeo(b, cfg().theme.metrics, cfg()).dial; }

bool ModernKnob::depthHandle(const Rect& b, double norm, Point& at) const {
    const KnobGeo g = knobGeo(b, cfg().theme.metrics, cfg());
    at = onCircle(g.cx, g.cy, knobAngle(cfg(), norm), g.ringRadius);
    return true;
}

void ModernKnob::paint(IRenderer& r, const Rect& b, const KnobState& s) const {
    const ModernConfig& c = cfg();
    const Theme& t = *s.theme;
    const Theme::Metrics& m = t.metrics;
    const bool large = s.large;
    const float labelPx = large ? c.knob.largeLabelPx : m.labelPx, valuePx = large ? c.knob.largeValuePx : m.valuePx;
    const KnobGeo g = knobGeo(b, m, c);
    const float cx = g.cx, cy = g.cy, R = g.radius;
    const Color ac = s.dimmed ? withAlpha(s.accent, 0.35f) : s.accent;
    const bool hot = s.hot || s.active;
    const float trackW = large ? c.knob.largeTrackWidth : c.knob.trackWidth;
    const float haloW = large ? c.knob.largeHaloWidth : c.knob.haloWidth;
    const float a1 = knobAngle(c, s.value);
    const float a0 = s.bipolar ? knobAngle(c, 0.5) : c.knob.startAngle;
    const float lo = std::min(a0, a1), hi = std::max(a0, a1);

    r.pushClip(b);
    if (s.label && *s.label)
        r.drawTextPx(fitText(r, s.label, labelPx, f(b.w) - 4.0f, FontWeight::Bold).c_str(), cx, f(b.y),
                     hot ? t.colors.text : t.colors.textDim, labelPx, Align::Center, FontWeight::Bold);
    // the track, then the value arc with its glow
    r.drawArc({cx, cy}, R, c.knob.startAngle, c.knob.startAngle + c.knob.sweep, t.colors.track, trackW);
    if (hi - lo > 0.01f) {
        if (c.effects.glow) {
            lit(r, c, 0.55f);
            r.drawArc({cx, cy}, R, lo, hi, halo(c, ac, 0.55f), haloW);
            lit(r, c, 1.7f);
            r.drawArc({cx, cy}, R, lo, hi, ac, trackW);
            lit(r, c, 1.0f);
            r.drawGlow(onCircle(cx, cy, a1, R), R * 0.3f + 7.0f, halo(c, ac, hot ? 0.5f : 0.3f));
            unlit(r);
        } else {
            r.drawArc({cx, cy}, R, lo, hi, ac, trackW);
        }
    }
    // the cap and its pointer
    const float body = R * c.knob.bodyRadius;
    if (c.shape.controlShadow > 0.0f) {
        const int d = static_cast<int>(body * 1.72f);
        r.drawShadow({static_cast<int>(cx) - d / 2, static_cast<int>(cy) - d / 2 + 3, d, d}, f(d) / 2,
                     c.shape.controlShadow, 0xA0000000u);
    }
    r.fillRadialGradient({cx - R * 0.12f, cy - R * 0.2f}, body, c.knob.bodyLight, c.knob.bodyDark);
    r.strokeCircle({cx, cy}, body, hairline(hot ? 0.34f : 0.2f), c.shape.stroke);
    r.drawLine(onCircle(cx, cy, a1, R * c.knob.pointerInner), onCircle(cx, cy, a1, R * c.knob.pointerOuter),
               c.knob.pointer ? c.knob.pointer : t.colors.text,
               large ? c.knob.pointerWidth + 0.4f : c.knob.pointerWidth);

    // modulation: the range from the base value to the live value, a dot per sounding voice, the depth handle
    const ModulationView& mod = s.mod;
    const float ringR = g.ringRadius;
    if (mod.any || mod.armed) {
        lit(r, c, 1.6f);
        if (mod.any) {
            const float m0 = knobAngle(c, mod.base), m1 = knobAngle(c, mod.shared);
            if (std::fabs(m1 - m0) > 0.004f)
                r.drawArc({cx, cy}, ringR, std::min(m0, m1), std::max(m0, m1), t.colors.modulation, 2.6f);
            lit(r, c, 2.0f);
            r.fillCircle(onCircle(cx, cy, m1, ringR), 2.6f, kWhite);
            lit(r, c, 1.0f);
            for (int v = 0; v < mod.voiceCount; ++v)
                r.fillCircle(onCircle(cx, cy, knobAngle(c, mod.voices[v]), ringR + 4.0f), 1.5f,
                             halo(c, t.colors.modulation, 0.55f));
        }
        if (mod.armed && mod.armedRoute) {
            const float d0 = knobAngle(c, mod.base);
            const float d1 = knobAngle(c, std::clamp(mod.base + mod.armedDepth, 0.0, 1.0));
            lit(r, c, 1.3f);
            r.drawArc({cx, cy}, ringR, std::min(d0, d1), std::max(d0, d1) + 0.0001f,
                      halo(c, t.colors.modulation, 0.8f / std::max(c.effects.glowStrength, 0.01f)), 3.2f);
        }
        unlit(r);
        if (mod.armed) {
            const double hn = std::clamp(mod.base + (mod.armedRoute ? mod.armedDepth : 0.0), 0.0, 1.0);
            const Point h = onCircle(cx, cy, knobAngle(c, hn), ringR);
            r.fillCircle(h, c.knob.handleRadius, rgb(14, 18, 32));
            r.strokeCircle(h, c.knob.handleRadius - 1.0f,
                           mod.overHandle || mod.handleActive ? kWhite : t.colors.modulation, 1.8f);
        }
    }
    // the value, or the entry field
    if (s.showValue) {
        const float vy = f(b.bottom()) - valuePx * kCap - 6.0f;
        if (s.editing) {
            const std::string text = std::string(s.editText) + (s.caret ? "|" : " ");
            r.fillRoundRect({b.x + 2, static_cast<int>(vy) - 4, b.w - 4, static_cast<int>(valuePx) + 6}, 6.0f,
                            withAlpha(ac, 0.22f));
            r.drawTextPx(text.c_str(), cx, vy, t.colors.text, valuePx, Align::Center, FontWeight::Bold);
        } else {
            r.drawTextPx(fitText(r, s.valueText, valuePx, f(b.w) - 2.0f).c_str(), cx, vy,
                         hot ? t.colors.text : t.colors.textDim, valuePx, Align::Center);
        }
    }
    if (s.focused)
        focusRing(r, c, t, g.dial, R + 8.0f, s.accent);
    r.popClip();
}

// --- slider ----------------------------------------------------------------------------------------------

namespace {
struct SliderGeo {
    Rect track;
    float labelPx, valuePx;
};

SliderGeo sliderGeo(const Rect& b, const Theme::Metrics& m, const ModernConfig& c, const SliderState& s) {
    SliderGeo g;
    g.labelPx = m.labelPx;
    g.valuePx = m.valuePx;
    const int thumb = static_cast<int>(std::ceil(c.slider.thumbRadius));
    const int th = static_cast<int>(std::lround(c.slider.trackHeight));
    if (s.vertical) {
        const int head = s.label && *s.label ? static_cast<int>(m.labelPx) + 6 : 0;
        const int foot = s.valueText && *s.valueText ? static_cast<int>(m.valuePx) + 6 : 0;
        g.track = {b.x + (b.w - th) / 2, b.y + head + thumb, th, std::max(8, b.h - head - foot - 2 * thumb)};
    } else {
        const int lw = s.label && *s.label ? static_cast<int>(c.slider.labelWidth) : 0;
        const int vw = s.valueText && *s.valueText ? static_cast<int>(c.slider.valueWidth) : 0;
        g.track = {b.x + lw + thumb, b.y + (b.h - th) / 2, std::max(10, b.w - lw - vw - 2 * thumb), th};
    }
    return g;
}
} // namespace

Rect ModernSlider::track(const Rect& b, const SliderState& s) const {
    return sliderGeo(b, cfg().theme.metrics, cfg(), s).track;
}

void ModernSlider::paint(IRenderer& r, const Rect& b, const SliderState& s) const {
    const ModernConfig& c = cfg();
    const Theme& t = *s.theme;
    const SliderGeo g = sliderGeo(b, t.metrics, c, s);
    const Rect tr = g.track;
    const bool hot = s.hot || s.active;
    const float rad = f(std::min(tr.w, tr.h)) * 0.5f;
    const float v = static_cast<float>(std::clamp(s.value, 0.0, 1.0));
    r.pushClip(b);
    r.fillRoundRect(tr, rad, t.colors.track);
    Rect fill;
    Point thumb;
    if (s.vertical) {
        const int len = static_cast<int>(std::lround(f(tr.h) * v));
        fill = {tr.x, tr.bottom() - len, tr.w, len};
        thumb = {f(tr.x) + f(tr.w) * 0.5f, f(tr.bottom()) - f(tr.h) * v};
    } else {
        fill = {tr.x, tr.y, static_cast<int>(std::lround(f(tr.w) * v)), tr.h};
        thumb = {f(tr.x) + f(tr.w) * v, f(tr.y) + f(tr.h) * 0.5f};
    }
    lit(r, c, 1.5f);
    r.fillRoundRect(fill, rad, s.accent);
    unlit(r);
    r.fillCircle(thumb, c.slider.thumbRadius, c.slider.thumb);
    const Color textCol = hot ? t.colors.text : t.colors.textDim;
    if (s.vertical) {
        if (s.label && *s.label)
            r.drawTextPx(fitText(r, s.label, g.labelPx, f(b.w), FontWeight::Bold).c_str(), f(b.x) + f(b.w) * 0.5f,
                         f(b.y), textCol, g.labelPx, Align::Center, FontWeight::Bold);
        if (s.valueText && *s.valueText)
            r.drawTextPx(fitText(r, s.valueText, g.valuePx, f(b.w)).c_str(), f(b.x) + f(b.w) * 0.5f,
                         f(b.bottom()) - g.valuePx * kCap - 1.0f, t.colors.textDim, g.valuePx, Align::Center);
    } else {
        if (s.label && *s.label)
            r.drawTextPx(fitText(r, s.label, g.labelPx, c.slider.labelWidth - 6.0f, FontWeight::Bold).c_str(), f(b.x),
                         capY(f(b.y), f(b.h), g.labelPx), textCol, g.labelPx, Align::Left, FontWeight::Bold);
        if (s.valueText && *s.valueText)
            r.drawTextPx(fitText(r, s.valueText, g.valuePx, c.slider.valueWidth - 4.0f).c_str(), f(b.right()),
                         capY(f(b.y), f(b.h), g.valuePx), t.colors.textDim, g.valuePx, Align::Right);
    }
    if (s.focused)
        focusRing(r, c, t, b.inset(1), 6.0f, s.accent);
    r.popClip();
}

// --- toggle ----------------------------------------------------------------------------------------------

void ModernToggle::paint(IRenderer& r, const Rect& b, const ToggleState& s) const {
    const ModernConfig& c = cfg();
    const Theme& t = *s.theme;
    const float sw = c.toggle.width, sh = c.toggle.height;
    const float sx = f(b.x), sy = f(b.y) + (f(b.h) - sh) * 0.5f;
    const Rect trackRect{static_cast<int>(sx), static_cast<int>(sy), static_cast<int>(sw), static_cast<int>(sh)};
    const float rad = sh * 0.5f;
    r.pushClip(b);
    if (s.on) {
        if (c.shape.controlShadow > 0.0f && c.effects.glow)
            r.drawShadow(trackRect, rad, c.shape.controlShadow, halo(c, s.accent, 0.4f));
        r.fillRoundRect(trackRect, rad, withAlpha(s.accent, 0.45f));
        r.strokeRoundRect(trackRect, rad, withAlpha(s.accent, 0.95f), c.shape.stroke);
    } else {
        r.fillRoundRect(trackRect, rad, hairline(s.hot ? 0.14f : 0.08f));
        r.strokeRoundRect(trackRect, rad, hairline(0.16f), c.shape.stroke);
    }
    const Point thumb{sx + (s.on ? sw - rad : rad), sy + rad};
    if (s.on && c.effects.glow) {
        lit(r, c, 1.8f);
        r.drawGlow(thumb, 12.0f, halo(c, s.accent, 0.6f));
        unlit(r);
    }
    r.fillCircle(thumb, rad - 2.5f, s.on ? c.toggle.thumbOn : c.toggle.thumbOff);
    const float px = t.metrics.labelPx + 0.5f;
    if (s.label && *s.label)
        r.drawTextPx(fitText(r, s.label, px, f(b.w) - sw - c.toggle.labelGap, FontWeight::Bold).c_str(),
                     sx + sw + c.toggle.labelGap, capY(f(b.y), f(b.h), px), s.on ? t.colors.text : t.colors.textDim, px,
                     Align::Left, FontWeight::Bold);
    if (s.focused)
        focusRing(r, c, t, trackRect, rad, s.accent);
    r.popClip();
}

// --- selector --------------------------------------------------------------------------------------------

void ModernSelector::paint(IRenderer& r, const Rect& b, const SelectorState& s) const {
    const ModernConfig& c = cfg();
    const Theme& t = *s.theme;
    const float rad = f(std::min(b.w, b.h)) * 0.5f;
    r.pushClip(b);
    r.fillRoundRect(b, rad, hairline(0.06f));
    r.strokeRoundRect(b, rad, hairline(0.12f), c.shape.stroke);
    const float px = t.metrics.labelPx;
    for (int i = 0; i < s.count; ++i) {
        const Rect cell = this->cell(b, s, i);
        const bool on = i == s.index, hot = i == s.hotIndex;
        const Rect face = cell.inset(2);
        const float fr = std::max(0.0f, rad - 2.0f);
        if (on) {
            if (c.shape.controlShadow > 0.0f && c.effects.glow)
                r.drawShadow(face, fr, c.shape.controlShadow * 0.8f, halo(c, s.accent, 0.3f));
            r.fillRoundRect(face, fr, withAlpha(s.accent, 0.34f));
            r.strokeRoundRect(face, fr, withAlpha(s.accent, 0.9f), c.shape.stroke);
        } else if (hot) {
            r.fillRoundRect(face, fr, hairline(0.08f));
        }
        if (s.icons && s.icons[i]) {
            const int ih = std::max(4, std::min(16, cell.h - 10)), iw = std::min(ih * 2, std::max(4, cell.w - 10));
            s.icons[i](r, {cell.x + (cell.w - iw) / 2, cell.y + (cell.h - ih) / 2, iw, ih},
                       on ? s.accent : (hot ? t.colors.text : t.colors.textDim), 1.6f);
            continue;
        }
        char fallback[24];
        const char* text = "";
        std::string enumText;
        if (s.labels) {
            text = s.labels[i];
        } else if (s.param && params::enumCount(*s.param) > 0) {
            enumText = std::string(params::enumLabel(*s.param, static_cast<std::size_t>(i)));
            text = enumText.c_str();
        } else {
            std::snprintf(fallback, sizeof(fallback), "%d", (s.param ? static_cast<int>(s.param->min) : 0) + i);
            text = fallback;
        }
        r.drawTextPx(fitText(r, text, px, f(cell.w) - 8.0f, FontWeight::Bold).c_str(), f(cell.x) + f(cell.w) * 0.5f,
                     capY(f(cell.y), f(cell.h), px), on ? s.accent : (hot ? t.colors.text : t.colors.textDim), px,
                     Align::Center, FontWeight::Bold);
    }
    if (s.focused)
        focusRing(r, c, t, b, rad, s.accent);
    r.popClip();
}

// --- button ----------------------------------------------------------------------------------------------

void ModernButton::paint(IRenderer& r, const Rect& b, const ButtonState& s) const {
    const ModernConfig& c = cfg();
    const Theme& t = *s.theme;
    const float rad = f(b.h) * 0.5f;
    const bool chip = s.variant == kButtonChip;
    const float px = chip ? t.metrics.valuePx - 0.5f : t.metrics.valuePx;
    r.pushClip(b);
    if (s.on) {
        if (chip && c.shape.controlShadow > 0.0f && c.effects.glow)
            r.drawShadow(b, rad, c.shape.controlShadow, halo(c, s.accent, 0.35f));
        r.fillRoundRect(b, rad, withAlpha(s.accent, chip ? 0.30f : 0.28f));
        r.strokeRoundRect(b, rad, withAlpha(s.accent, chip ? 0.95f : 0.8f), c.shape.stroke);
    } else {
        const float fillA = s.active ? 0.2f : s.hot ? (chip ? 0.12f : 0.13f) : (chip ? 0.05f : 0.07f);
        r.fillRoundRect(b, rad, hairline(fillA));
        r.strokeRoundRect(b, rad, hairline(s.hot ? (chip ? 0.28f : 0.3f) : (chip ? 0.12f : 0.14f)), c.shape.stroke);
    }
    const Color ink = s.on ? s.accent : (s.hot ? t.colors.text : t.colors.textDim);
    float textX = f(b.x) + f(b.w) * 0.5f, room = f(b.w) - (chip ? 14.0f : 12.0f);
    if (s.icon) {
        // alone it is centered; beside a label it takes the left end
        const bool text = s.label && *s.label;
        const int ih = std::max(4, std::min(16, b.h - 10)), iw = std::min(ih * 2, std::max(4, b.w - 12));
        const int ix = text ? b.x + 8 : b.x + (b.w - iw) / 2;
        s.icon(r, {ix, b.y + (b.h - ih) / 2, iw, ih}, ink, 1.6f);
        if (text) {
            textX = f(ix + iw) + (f(b.right() - ix - iw)) * 0.5f;
            room = f(b.right() - ix - iw) - 10.0f;
        }
    }
    if (s.label && *s.label)
        r.drawTextPx(fitText(r, s.label, px, room, FontWeight::Bold).c_str(), textX, capY(f(b.y), f(b.h), px), ink, px,
                     Align::Center, FontWeight::Bold);
    if (s.focused)
        focusRing(r, c, t, b, rad, s.accent);
    r.popClip();
}

// --- lamp ------------------------------------------------------------------------------------------------

void ModernLed::paint(IRenderer& r, const Rect& b, const LedState& s) const {
    const ModernConfig& c = cfg();
    const Theme& t = *s.theme;
    const float cy = f(b.y) + f(b.h) * 0.5f, rad = std::clamp(f(b.h) * 0.5f - 4.0f, 3.0f, 5.0f);
    const Point at{f(b.x) + rad + 4.0f, cy};
    const float k = s.on ? std::clamp(s.level, 0.0f, 1.0f) : 0.0f;
    r.pushClip(b.expand(margin()));
    r.fillCircle(at, rad + 1.5f, withAlpha(rgb(0, 0, 0), 0.45f));
    r.fillCircle(at, rad, mix(withAlpha(s.accent, 0.22f), s.accent, k));
    if (k > 0.0f) {
        if (c.effects.glow) {
            lit(r, c, 1.8f * k);
            r.drawGlow(at, rad * 3.2f, halo(c, s.accent, 0.7f * k));
            unlit(r);
        }
        r.fillCircle({at.x - rad * 0.25f, at.y - rad * 0.3f}, rad * 0.35f, withAlpha(kWhite, 0.55f * k));
    }
    if (s.label && *s.label) {
        const float px = t.metrics.labelPx + 0.5f;
        r.drawTextPx(fitText(r, s.label, px, f(b.w) - rad * 2.0f - 14.0f, FontWeight::Bold).c_str(), at.x + rad + 8.0f,
                     capY(f(b.y), f(b.h), px), s.on ? t.colors.text : t.colors.textDim, px, Align::Left,
                     FontWeight::Bold);
    }
    r.popClip();
}

// --- keyboard --------------------------------------------------------------------------------------------

void ModernKeyboard::paint(IRenderer& r, const Rect& b, const KeyboardState& s) const {
    const ModernConfig& c = cfg();
    const Theme& t = *s.theme;
    const keyboard::Layout lay = layout();
    r.pushClip(b);
    r.fillRect(b, withAlpha(rgb(0, 0, 0), 0.4f));
    for (int pass = 0; pass < 2; ++pass)
        for (int k = s.first; k <= s.last; ++k) {
            if (keyboard::isBlack(k) != (pass == 1))
                continue;
            const keyboard::KeyRect kr = keyboard::keyRect(b, s.first, s.last, k, lay);
            const bool down = s.down && s.down[k], sounding = s.lit && s.lit[k];
            const Rect cell{static_cast<int>(kr.x0) + 1, static_cast<int>(kr.y0), static_cast<int>(kr.x1 - kr.x0) - 1,
                            static_cast<int>(kr.y1 - kr.y0) - 1};
            Color col = pass == 1 ? rgb(24, 28, 42) : rgb(214, 220, 236);
            if (s.hotKey == k && !down)
                col = pass == 1 ? rgb(40, 46, 66) : rgb(232, 238, 252);
            if (down || sounding)
                col = mix(col, s.accent, down ? 0.7f : 0.55f);
            r.fillRoundRect(cell, 3.0f, col);
            if (pass == 1)
                r.strokeRoundRect(cell, 3.0f, hairline(0.18f), c.shape.stroke);
            if (sounding && c.effects.glow) {
                lit(r, c, 1.4f);
                r.fillRoundRect(cell.inset(1), 2.0f, halo(c, s.accent, 0.35f));
                unlit(r);
            }
        }
    (void)t;
    r.popClip();
}

// --- dropdown --------------------------------------------------------------------------------------------

void ModernDropdown::paint(IRenderer& r, const Rect& b, const DropdownState& s) const {
    const ModernConfig& c = cfg();
    const Theme& t = *s.theme;
    const float rad = c.shape.fieldRadius;
    const float px = t.metrics.valuePx;
    r.pushClip(b);
    r.fillRoundRect(b, rad, hairline(s.hot || s.open ? 0.12f : 0.07f));
    r.strokeRoundRect(b, rad, s.open ? withAlpha(t.colors.accent, 0.8f) : hairline(s.hot ? 0.3f : 0.14f),
                      c.shape.stroke);
    float tx = f(b.x) + 10.0f;
    if (s.label && *s.label) {
        r.drawTextPx(s.label, tx, capY(f(b.y), f(b.h), t.metrics.labelPx - 0.5f), t.colors.textFaint,
                     t.metrics.labelPx - 0.5f, Align::Left, FontWeight::Bold);
        tx += r.textWidthPx(s.label, t.metrics.labelPx - 0.5f, FontWeight::Bold) + 8.0f;
    }
    r.drawTextPx(fitText(r, s.valueText, px, f(b.right()) - tx - 24.0f).c_str(), tx, capY(f(b.y), f(b.h), px),
                 t.colors.text, px);
    const float cx = f(b.right()) - 13.0f, cy = f(b.y) + f(b.h) * 0.5f;
    r.drawLine({cx - 3.5f, cy - 1.5f}, {cx, cy + 2.0f}, t.colors.textDim, 1.6f);
    r.drawLine({cx, cy + 2.0f}, {cx + 3.5f, cy - 1.5f}, t.colors.textDim, 1.6f);
    if (s.focused)
        focusRing(r, c, t, b, rad, t.colors.focus);
    r.popClip();
}

// --- stepper ---------------------------------------------------------------------------------------------

void ModernStepper::paint(IRenderer& r, const Rect& b, const StepperState& s) const {
    const ModernConfig& c = cfg();
    const Theme& t = *s.theme;
    const float rad = f(b.h) * 0.5f;
    const float px = t.metrics.valuePx;
    r.pushClip(b);
    r.fillRoundRect(b, rad, hairline(0.07f));
    r.strokeRoundRect(b, rad, hairline(0.14f), c.shape.stroke);
    for (int k = 0; k < 2; ++k) {
        const Rect part = k == 0 ? minusPart(b) : plusPart(b);
        const bool hot = s.hotPart == (k == 0 ? -1 : 1);
        if (hot)
            r.fillRoundRect(part.inset(2), std::max(0.0f, rad - 2.0f), hairline(0.12f));
        const Point mid = part.center();
        const Color col = hot ? t.colors.text : t.colors.textDim;
        r.drawLine({mid.x - 4.0f, mid.y}, {mid.x + 4.0f, mid.y}, col, 1.6f);
        if (k == 1)
            r.drawLine({mid.x, mid.y - 4.0f}, {mid.x, mid.y + 4.0f}, col, 1.6f);
    }
    std::string text = s.valueText;
    if (s.label && *s.label)
        text = std::string(s.label) + "  " + text;
    const Rect mid = minusPart(b), pl = plusPart(b);
    r.drawTextPx(fitText(r, text, px, f(pl.x - mid.right()) - 4.0f, FontWeight::Bold).c_str(),
                 f(mid.right()) + f(pl.x - mid.right()) * 0.5f, capY(f(b.y), f(b.h), px), t.colors.text, px,
                 Align::Center, FontWeight::Bold);
    if (s.focused)
        focusRing(r, c, t, b, rad, t.colors.focus);
    r.popClip();
}

// --- panel, text field -----------------------------------------------------------------------------------

Margin ModernPanel::margin() const { return shadowMargin(cfg()); }

void ModernPanel::paint(IRenderer& r, const Rect& b, const PanelState& s) const {
    const ModernConfig& c = cfg();
    const Theme& t = *s.theme;
    if (c.shape.panelShadow) {
        r.drawPanel(b, c.shape.panelRadius, t.colors.panel);
    } else {
        r.fillRoundRect(b, c.shape.panelRadius, t.colors.panel);
        r.strokeRoundRect(b, c.shape.panelRadius, hairline(0.14f), c.shape.stroke);
    }
    if (s.title && *s.title) {
        lit(r, c, 1.8f);
        r.fillCircle({f(b.x) + 18.0f, f(b.y) + 20.0f}, 2.5f, s.accent);
        unlit(r);
        r.drawTextPx(s.title, f(b.x) + 28.0f, f(b.y) + 15.0f, t.colors.textDim, t.metrics.titlePx, Align::Left,
                     FontWeight::Bold);
    }
}

void ModernTextField::paint(IRenderer& r, const Rect& b, const TextFieldState& s) const {
    const ModernConfig& c = cfg();
    const Theme& t = *s.theme;
    const float px = t.metrics.valuePx + 1.0f;
    r.pushClip(b);
    r.fillRoundRect(b, c.shape.fieldRadius, hairline(0.09f));
    r.strokeRoundRect(b, c.shape.fieldRadius, withAlpha(t.colors.accent, 0.9f), c.shape.stroke + 0.2f);
    const std::string shown = std::string(s.text) + (s.caret ? "|" : " ");
    r.drawTextPx(fitText(r, shown, px, f(b.w) - 20.0f).c_str(), f(b.x) + 10.0f, capY(f(b.y), f(b.h), px), t.colors.text,
                 px);
    r.popClip();
}

// --- popup, tooltip --------------------------------------------------------------------------------------

Margin ModernPopup::margin() const { return shadowMargin(cfg()); }
Margin ModernTooltip::margin() const { return shadowMargin(cfg()); }

Rect ModernPopup::layout(IRenderer& r, const PopupState& s, int viewW, int viewH, std::vector<Rect>& rects) const {
    const ModernConfig& c = cfg();
    const Theme::Metrics& m = s.theme->metrics;
    const std::vector<PopupItem>& items = *s.items;
    const int pad = static_cast<int>(c.popup.padding);
    const int n = static_cast<int>(items.size());
    const int cols = std::max(1, s.columns);
    const int rows = std::max(1, (n + cols - 1) / cols);
    float maxW = f(s.anchor.w);
    for (const PopupItem& it : items)
        if (!it.separator)
            maxW = std::max(maxW, r.textWidthPx(it.label.c_str(), m.popupPx) + c.popup.itemPadding);
    const int itemW = static_cast<int>(std::ceil(maxW));
    const int sepH = static_cast<int>(c.popup.separatorHeight);
    // column-major, so a long list reads down its columns
    std::vector<int> colY(static_cast<std::size_t>(cols), 0);
    rects.assign(static_cast<std::size_t>(n), Rect{});
    for (int i = 0; i < n; ++i) {
        const int col = i / rows;
        const int h = items[static_cast<std::size_t>(i)].separator ? sepH : m.popupRow;
        rects[static_cast<std::size_t>(i)] = {pad + col * itemW, pad + colY[static_cast<std::size_t>(col)], itemW, h};
        colY[static_cast<std::size_t>(col)] += h;
    }
    const int w = itemW * cols + pad * 2, h = *std::max_element(colY.begin(), colY.end()) + pad * 2;
    int x = s.anchor.x, y = s.anchor.empty() ? s.anchor.y : s.anchor.bottom() + 4;
    if (y + h > viewH - 4)
        y = std::max(4, s.anchor.y - h - (s.anchor.empty() ? 0 : 4));
    if (x + w > viewW - 4)
        x = std::max(4, viewW - 4 - w);
    for (Rect& it : rects) {
        it.x += x;
        it.y += y;
    }
    return {x, y, w, h};
}

void ModernPopup::paint(IRenderer& r, const Rect& b, const PopupState& s) const {
    const ModernConfig& c = cfg();
    const Theme& t = *s.theme;
    const float px = t.metrics.popupPx;
    if (c.shape.panelShadow) {
        r.drawPanel(b, c.shape.popupRadius, t.colors.popup);
    } else {
        r.fillRoundRect(b, c.shape.popupRadius, t.colors.popup);
        r.strokeRoundRect(b, c.shape.popupRadius, hairline(0.14f), c.shape.stroke);
    }
    const std::vector<PopupItem>& items = *s.items;
    for (std::size_t i = 0; i < items.size() && i < s.itemRects->size(); ++i) {
        const Rect& cell = (*s.itemRects)[i];
        const PopupItem& it = items[i];
        if (it.separator) {
            r.fillRect({cell.x + 8, cell.y + cell.h / 2, cell.w - 16, 1}, t.colors.hairline);
            continue;
        }
        const bool hot = static_cast<int>(i) == s.hot && it.enabled;
        if (hot)
            r.fillRoundRect(cell.inset(1), c.shape.itemRadius, withAlpha(t.colors.accent, 0.22f));
        if (static_cast<int>(i) == s.selected)
            r.fillCircle({f(cell.x) + 12.0f, f(cell.y) + f(cell.h) * 0.5f}, 3.0f, t.colors.accent);
        r.drawTextPx(fitText(r, it.label, px, f(cell.w) - c.popup.itemPadding).c_str(), f(cell.x) + 24.0f,
                     capY(f(cell.y), f(cell.h), px),
                     !it.enabled ? t.colors.textFaint
                     : hot       ? t.colors.text
                                 : t.colors.textDim,
                     px);
    }
}

namespace {
// Vertical layout of the tooltip text, from the metrics (28 and 44 px at the default sizes).
float tooltipBodyY(const Theme::Metrics& m) { return 9.0f + m.valuePx + 0.5f + 6.0f; }
int tooltipHeight(const Theme::Metrics& m, bool body) {
    return static_cast<int>(body ? tooltipBodyY(m) + m.valuePx + 6.0f : 8.0f + m.valuePx + 0.5f + 8.0f);
}
} // namespace

Rect ModernTooltip::layout(IRenderer& r, const TooltipState& s, int viewW, int viewH) const {
    const Theme::Metrics& m = s.theme->metrics;
    const bool body = s.text && *s.text;
    const float w = std::max(r.textWidthPx(s.title, m.valuePx + 0.5f, FontWeight::Bold),
                             body ? r.textWidthPx(s.text, m.valuePx) : 0.0f) +
                    24.0f;
    // never wider than the view (paint() fits the text to what is left); the height follows the text size
    const int iw = std::min(static_cast<int>(w), std::max(0, viewW - 8)), ih = tooltipHeight(m, body);
    int x = static_cast<int>(s.pointer.x) + 14, y = static_cast<int>(s.pointer.y) + 18;
    if (x + iw > viewW - 4)
        x = std::max(4, viewW - 4 - iw);
    if (y + ih > viewH - 4)
        y = std::max(4, static_cast<int>(s.pointer.y) - ih - 14);
    return {x, y, iw, ih};
}

void ModernTooltip::paint(IRenderer& r, const Rect& b, const TooltipState& s) const {
    const ModernConfig& c = cfg();
    const Theme& t = *s.theme;
    if (c.shape.panelShadow) {
        r.drawPanel(b, c.shape.tooltipRadius, t.colors.tooltip);
    } else {
        r.fillRoundRect(b, c.shape.tooltipRadius, t.colors.tooltip);
        r.strokeRoundRect(b, c.shape.tooltipRadius, hairline(0.14f), c.shape.stroke);
    }
    const bool body = s.text && *s.text;
    const float room = f(b.w) - 24.0f;
    r.drawTextPx(fitText(r, s.title, t.metrics.valuePx + 0.5f, room, FontWeight::Bold).c_str(), f(b.x) + 12.0f,
                 f(b.y) + (body ? 9.0f : 8.0f), t.colors.text, t.metrics.valuePx + 0.5f, Align::Left, FontWeight::Bold);
    if (body)
        r.drawTextPx(fitText(r, s.text, t.metrics.valuePx, room).c_str(), f(b.x) + 12.0f,
                     f(b.y) + tooltipBodyY(t.metrics), t.colors.textDim, t.metrics.valuePx);
}

} // namespace spux::inline v1::ui::modern
