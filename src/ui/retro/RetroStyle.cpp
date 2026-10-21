#include "spux/ui/retro/RetroStyle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace spux::inline v1::ui::retro {

namespace {

constexpr double kPi = 3.14159265358979323846;

const Theme& themeOf(const Theme* t, const RetroConfig& c) { return t ? *t : c.theme; }

// A widget whose accent is the theme's default accent has no color of its own.
bool hasOwnColor(const Theme& t, Color accent) { return accent != 0 && accent != t.colors.accent; }

Color orElse(Color c, Color fallback) { return c ? c : fallback; }
Color lift(Color c, float t) { return ui::mix(c, 0xFFFFFFFFu, t); }
Color shade(Color c, float k) { return ui::mix(c, 0xFF000000u, 1.0f - k); }

int roundInt(double v) { return static_cast<int>(std::lround(v)); }

const BitmapFont* titleFontOf(const RetroConfig& c) { return c.titleFont ? c.titleFont : c.font; }
int capOf(const BitmapFont* f) { return f ? f->capHeight : 0; }
int widthOf(const BitmapFont* f, const char* s) { return f ? f->textWidth(s) : 0; }

// The top row of a capital that sits in the middle of a row of height `h` starting at y.
int capTopIn(int y, int h, int cap) { return y + (h - cap) / 2; }

void put(PixelRenderer& r, const BitmapFont* f, const char* s, int x, int capTop, Color c) {
    if (f && s && *s)
        r.text(*f, s, x, capTop, c);
}
void putCentered(PixelRenderer& r, const BitmapFont* f, const char* s, int centerX, int capTop, Color c) {
    if (f && s && *s)
        r.textCentered(*f, s, centerX, capTop, c);
}
// The text in a row of a rectangle, vertically centered.
void putInRow(PixelRenderer& r, const BitmapFont* f, const char* s, int x, const Rect& row, Color c) {
    put(r, f, s, x, capTopIn(row.y, row.h, capOf(f)), c);
}
void putCenteredInRow(PixelRenderer& r, const BitmapFont* f, const char* s, int centerX, const Rect& row, Color c) {
    putCentered(r, f, s, centerX, capTopIn(row.y, row.h, capOf(f)), c);
}

Rect clipOf(const Rect& b, const Margin& m) { return b.expand(m); }

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

bool isBipolar(const params::ParamInfo* p) { return p && p->min < 0.0 && p->max > 0.0; }

// The bevelled face of a key: an outline, the face, light and dark edges (sunken while pressed).
void paintFace(PixelRenderer& r, const Rect& f, Color base, bool on, bool pressed, bool hot, const RetroConfig& c) {
    const RetroConfig::Button& b = c.button;
    Color face = on ? b.faceOn : base;
    if (hot && !pressed)
        face = lift(face, 0.12f);
    Rect inner = f;
    if (b.outline) {
        r.fillRect(f, b.edge);
        inner = f.inset(1);
    }
    r.fillRect(inner, face);
    r.bevel(inner, b.light, b.dark, pressed);
}

// The recessed display of a dropdown, a stepper, a text field and the readout.
void paintDisplay(PixelRenderer& r, const Rect& f, bool hot, Color edgeOverride, const RetroConfig& c) {
    const RetroConfig::Display& d = c.display;
    r.fillRect(f, d.fill);
    if (edgeOverride) {
        r.strokeRect(f, edgeOverride);
    } else if (d.sunken) {
        r.bevel(f, c.button.light, c.button.dark, true);
    } else {
        r.strokeRect(f, hot ? lift(d.edge, 0.3f) : d.edge);
    }
}

// A downward chevron as a small triangle, its top row `w` wide, centered on x.
void triangleDown(PixelRenderer& r, int cx, int y, int w, Color c) {
    for (int i = 0; w - 2 * i > 0; ++i)
        r.hLine(cx - w / 2 + i, y + i, w - 2 * i, c);
}

// A chevron of lines pointing left or right, centered on (cx, cy).
void chevron(PixelRenderer& r, int cx, int cy, int dir, Color c) {
    r.drawLine({static_cast<float>(cx - 2 * dir), static_cast<float>(cy - 4)},
               {static_cast<float>(cx + 2 * dir), static_cast<float>(cy)}, c);
    r.drawLine({static_cast<float>(cx + 2 * dir), static_cast<float>(cy)},
               {static_cast<float>(cx - 2 * dir), static_cast<float>(cy + 4)}, c);
}

void focusRect(PixelRenderer& r, const Rect& b, const Theme& theme) {
    r.strokeRect(b.inset(1), withAlpha(theme.colors.focus, 0.9f));
}

} // namespace

// --- knob ----------------------------------------------------------------------------------------------------------

namespace {

struct KnobGeo {
    int cx, cy, r;
};

KnobGeo knobGeo(const Rect& b, const RetroConfig& c) {
    const RetroConfig::Knob& k = c.knob;
    const int availH = b.h - k.topRow - k.bottomRow;
    // Room around the dial for the printed scale: the ticks reach 8 pixels beyond the radius, the index square 14.
    const int room = k.ticks ? (k.indexSquare ? 14 : 9) : 3;
    KnobGeo g;
    g.cx = b.x + b.w / 2;
    g.cy = b.y + k.topRow + availH / 2;
    g.r = std::max(3, (std::min(b.w, availH) - 1) / 2 - room);
    return g;
}

Point centerOf(const KnobGeo& k) { return {static_cast<float>(k.cx) + 0.5f, static_cast<float>(k.cy) + 0.5f}; }

void radial(PixelRenderer& r, const KnobGeo& k, double angle, int from, int to, Color c, float width = 1.0f) {
    const double ca = std::cos(angle), sa = std::sin(angle);
    r.drawLine({static_cast<float>(k.cx + roundInt(ca * from)), static_cast<float>(k.cy + roundInt(sa * from))},
               {static_cast<float>(k.cx + roundInt(ca * to)), static_cast<float>(k.cy + roundInt(sa * to))}, c, width);
}

// The soft drop shadow: translucent discs offset down and to the right.
void knobShadow(PixelRenderer& r, const KnobGeo& k) {
    const int cx = k.cx, cy = k.cy, R = k.r;
    auto disc = [&](int dx, int dy, int rad, Color c) {
        r.fillCircle({static_cast<float>(cx + dx) + 0.5f, static_cast<float>(cy + dy) + 0.5f}, static_cast<float>(rad),
                     c);
    };
    disc(5, 6, R + 2, 0x14000000);
    disc(4, 5, R + 1, 0x22000000);
    disc(3, 4, R, 0x30000000);
    disc(2, 2, R, 0x40000000);
}

void paintFlutedKnob(PixelRenderer& r, const KnobGeo& k, const KnobState& s, const RetroConfig& c, const Theme& theme,
                     double angle) {
    const RetroConfig::Knob& kc = c.knob;
    const bool hot = s.hot || s.active;
    const int cx = k.cx, cy = k.cy, R = k.r;
    const Point mid = centerOf(k);
    auto disc = [&](int dx, int dy, int rad, Color col) {
        r.fillCircle({mid.x + static_cast<float>(dx), mid.y + static_cast<float>(dy)}, static_cast<float>(rad), col);
    };
    // The skirt: a dark bezel and the fluted ring.
    disc(0, 0, R + 1, kc.bezel);
    disc(0, 0, R, kc.skirt);
    disc(0, 0, R - 1, kc.rim);
    for (int deg = 0; deg < 360; deg += 30) {
        const double a = deg * kPi / 180.0;
        radial(r, k, a, R - 5, R - 1, kc.flute);
    }
    // The conical top face, lit from the upper left: a lighter sheen and a darker rim.
    const int faceR = R - 5;
    const Color face = hot ? lift(kc.face, 0.2f) : kc.face, sheen = hot ? lift(kc.sheen, 0.2f) : kc.sheen;
    disc(0, 0, faceR, face);
    disc(-1, -1, faceR - 1, sheen);
    disc(-2, -2, faceR - 5, 0x30FFFFFF);
    r.strokeCircle(mid, static_cast<float>(faceR + 1), kc.faceEdge, 1.0f);
    // The colored cap.
    if (hasOwnColor(theme, s.accent)) {
        disc(0, 0, faceR / 2, s.accent);
        r.strokeCircle(mid, static_cast<float>(faceR / 2 + 1), 0xFF303336, 1.0f);
    }
    radial(r, k, angle, 0, R - 2, orElse(kc.pointer, c.ink), static_cast<float>(kc.pointerWidth));
    (void)cx;
    (void)cy;
}

// A flat disc: a dark rim, a lit edge at the upper left and a shadowed one at the lower right, a pointer.
void paintFlatKnob(PixelRenderer& r, const KnobGeo& k, const KnobState& s, const RetroConfig& c, const Theme& theme,
                   double angle) {
    const RetroConfig::Knob& kc = c.knob;
    const bool hot = s.hot || s.active;
    const Point mid = centerOf(k);
    const float R = static_cast<float>(k.r);
    Color face = kc.face;
    if (hot)
        face = lift(face, 0.15f);
    r.fillCircle(mid, R + 1.0f, kc.bezel);
    r.fillCircle(mid, R, face);
    r.drawArc(mid, R - 1.0f, static_cast<float>(kPi), static_cast<float>(1.5 * kPi), c.button.light, 1.0f);
    r.drawArc(mid, R - 1.0f, 0.0f, static_cast<float>(0.5 * kPi), c.button.dark, 1.0f);
    // The colored cap.
    if (hasOwnColor(theme, s.accent))
        r.fillCircle(mid, R * 0.5f + 1.0f, s.accent);
    radial(r, k, angle, 0, k.r - 2, orElse(kc.pointer, c.ink), static_cast<float>(kc.pointerWidth));
}

} // namespace

Rect RetroKnob::hitArea(const Rect& b) const {
    const KnobGeo g = knobGeo(b, cfg());
    const int h = g.r + 4;
    return Rect{g.cx - h, g.cy - h, 2 * h + 1, 2 * h + 1}.intersect(b);
}

void RetroKnob::paint(IRenderer& rr, const Rect& b, const KnobState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r)
        return;
    const RetroConfig& c = cfg();
    const RetroConfig::Knob& kc = c.knob;
    const Theme& theme = themeOf(s.theme, c);
    const KnobGeo k = knobGeo(b, c);
    const double start = kc.startDeg * kPi / 180.0, sweep = kc.sweepDeg * kPi / 180.0;
    const double norm = std::clamp(s.value, 0.0, 1.0);
    const Point mid = centerOf(k);
    r->pushClip(b);

    // The printed scale: 11 ticks across the sweep and the index square above the middle tick.
    if (kc.ticks) {
        const Color tick = orElse(kc.tick, c.ink);
        for (int i = 0; i < 11; ++i)
            radial(*r, k, start + sweep * i / 10.0, k.r + 4, k.r + 8, tick);
        if (kc.indexSquare)
            r->fillRect({k.cx - 2, k.cy - k.r - 14, 4, 4}, tick);
    }
    // The live modulated value: the swing between the knob's own value and the modulated one, just outside the dial.
    if (s.mod.any) {
        const double lo = std::min(s.mod.base, s.mod.shared), hi = std::max(s.mod.base, s.mod.shared);
        r->drawArc(mid, static_cast<float>(k.r + 3), static_cast<float>(start + sweep * lo),
                   static_cast<float>(start + sweep * hi + 0.02), theme.colors.modulation, 1.0f);
    }
    if (kc.shadow)
        knobShadow(*r, k);
    const double angle = start + norm * sweep;
    if (kc.shape == KnobShape::Fluted)
        paintFlutedKnob(*r, k, s, c, theme, angle);
    else
        paintFlatKnob(*r, k, s, c, theme, angle);
    if (s.pickable)
        r->strokeCircle(mid, static_cast<float>(k.r + std::min(10, kc.ticks ? 10 : 2)), kc.pick, 1.0f);
    if (s.focused)
        r->strokeCircle(mid, static_cast<float>(k.r + 2), withAlpha(theme.colors.focus, 0.9f), 1.0f);

    // The label and the value, in the rows the config reserves.
    const int topRowY = b.y, bottomRowY = b.bottom() - kc.bottomRow;
    const Rect labelRow = kc.labelBelow ? Rect{b.x, bottomRowY, b.w, kc.bottomRow} : Rect{b.x, topRowY, b.w, kc.topRow};
    const Rect valueRow = kc.labelBelow ? Rect{b.x, topRowY, b.w, kc.topRow} : Rect{b.x, bottomRowY, b.w, kc.bottomRow};
    if (s.label && *s.label && labelRow.h > 0)
        putCenteredInRow(*r, c.font, s.label, k.cx, labelRow, c.ink);
    if (kc.showValue && s.showValue && valueRow.h > 0) {
        const std::string text = s.editing ? std::string(s.editText) + (s.caret ? "_" : "") : std::string(s.valueText);
        putCenteredInRow(*r, c.font, text.c_str(), k.cx, valueRow,
                         s.editing ? c.ink : (s.dimmed ? c.inkDim : c.valueInk));
    }
    r->popClip();
}

// --- slider --------------------------------------------------------------------------------------------------------

namespace {

struct SliderGeo {
    int cx, cy;   // the slot's center line
    int lo, hi;   // travel of the cap's center along the slot: lo = value 0, hi = value 1 (screen coordinates)
    int half;     // half the cap's length along the slot, with its rim
    int capShort; // the cap's size across the slot
    bool vertical;
};

SliderGeo sliderGeo(const Rect& b, bool vertical, const RetroConfig& c) {
    const RetroConfig::Slider& sl = c.slider;
    SliderGeo g;
    g.vertical = vertical;
    g.half = sl.capLong / 2 + 1;
    g.cx = b.x + b.w / 2;
    g.cy = b.y + b.h / 2;
    g.capShort = std::max(3, std::min(sl.capShort, (vertical ? b.w : b.h) - 2));
    if (vertical) {
        g.lo = b.bottom() - sl.bottomRow - g.half;
        g.hi = b.y + sl.topRow + g.half;
    } else {
        g.lo = b.x + g.half;
        g.hi = b.right() - g.half;
    }
    return g;
}

} // namespace

Rect RetroSlider::track(const Rect& b, const SliderState& s) const {
    const SliderGeo g = sliderGeo(b, s.vertical, cfg());
    if (s.vertical)
        return {g.cx - g.capShort / 2, g.hi, g.capShort, std::max(1, g.lo - g.hi)};
    return {g.lo, g.cy - g.capShort / 2, std::max(1, g.hi - g.lo), g.capShort};
}

void RetroSlider::paint(IRenderer& rr, const Rect& b, const SliderState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r)
        return;
    const RetroConfig& c = cfg();
    const RetroConfig::Slider& sl = c.slider;
    const Theme& theme = themeOf(s.theme, c);
    const SliderGeo g = sliderGeo(b, s.vertical, c);
    const bool hot = s.hot || s.active;
    const double norm = std::clamp(s.value, 0.0, 1.0);
    const int span = s.vertical ? g.lo - g.hi : g.hi - g.lo;
    const int along = roundInt(norm * span);
    const int zero = isBipolar(s.param) ? span / 2 : 0; // the value the fill starts from
    const Color fillColor = hasOwnColor(theme, s.accent) ? s.accent : sl.fill;
    r->pushClip(b);

    // The slot, its fill from the zero point to the cap, and the printed scale beside it (vertical).
    if (s.vertical) {
        const Rect slot{g.cx - 2, g.hi - g.half, 5, g.lo - g.hi + 2 * g.half + 1};
        r->fillRect(slot, sl.slotEdge);
        r->fillRect(slot.inset(1), sl.slot);
        const int y0 = g.lo - std::max(along, zero), y1 = g.lo - std::min(along, zero);
        r->fillRect({g.cx - 1, y0, 3, y1 - y0 + 1}, fillColor);
        for (int i = 0; i <= 10; ++i) {
            const int y = g.lo - roundInt(span * i / 10.0), len = i % 5 == 0 ? 5 : 3;
            r->hLine(g.cx - 4 - len, y, len, c.inkDim);
        }
        const Rect cap{g.cx - g.capShort / 2, g.lo - along - sl.capLong / 2, g.capShort, sl.capLong};
        paintFace(*r, cap, hasOwnColor(theme, s.accent) ? s.accent : c.button.face, false, s.active, hot, c);
        r->hLine(cap.x + 2, g.lo - along, cap.w - 4, c.button.dark);
        const Rect topRow{b.x, b.y, b.w, sl.topRow}, bottomRow{b.x, b.bottom() - sl.bottomRow, b.w, sl.bottomRow};
        if (s.valueText && *s.valueText && sl.topRow > 0)
            putCenteredInRow(*r, c.font, s.valueText, g.cx, topRow, c.valueInk);
        if (s.label && *s.label && sl.bottomRow > 0)
            putCenteredInRow(*r, c.font, s.label, g.cx, bottomRow, c.ink);
    } else {
        const Rect slot{g.lo - g.half, g.cy - 2, g.hi - g.lo + 2 * g.half + 1, 5};
        r->fillRect(slot, sl.slotEdge);
        r->fillRect(slot.inset(1), sl.slot);
        const int x0 = g.lo + std::min(along, zero), x1 = g.lo + std::max(along, zero);
        r->fillRect({x0, g.cy - 1, x1 - x0 + 1, 3}, fillColor);
        const Rect cap{g.lo + along - sl.capLong / 2, g.cy - g.capShort / 2, sl.capLong, g.capShort};
        paintFace(*r, cap, hasOwnColor(theme, s.accent) ? s.accent : c.button.face, false, s.active, hot, c);
        r->vLine(g.lo + along, cap.y + 2, cap.h - 4, c.button.dark);
    }
    if (s.focused)
        focusRect(*r, b, theme);
    r->popClip();
}

// --- toggle --------------------------------------------------------------------------------------------------------

namespace {

// A lever in a plate: on is the lever down, off up. `cy` is the middle of the plate.
void paintLever(PixelRenderer& r, int cx, int cy, const ToggleState& s, const RetroConfig& c) {
    const RetroConfig::Lever& lv = c.lever;
    // The soft shadow, the frame, the metal and the well.
    r.fillRect({cx - 7, cy - 18, 18, 42}, 0x22000000);
    r.fillRect({cx - 8, cy - 19, 17, 41}, 0x30000000);
    r.fillRect({cx - 8, cy - 20, 16, 40}, lv.edge);
    r.fillRect({cx - 7, cy - 19, 14, 38}, lv.metal);
    r.fillRect({cx - 5, cy - 17, 10, 34}, lv.well);
    // The handle: a shadow, an edge, the face with an inner square, and a groove across it.
    const int hy = s.on ? cy + 2 : cy - 16;
    r.fillRect({cx - 6, hy + 2, 14, 14}, 0x40000000);
    r.fillRect({cx - 7, hy, 14, 14}, lv.handleEdge);
    r.fillRect({cx - 6, hy + 1, 12, 12}, (s.hot || s.active) ? lift(lv.handleFace, 0.25f) : lv.handleFace);
    r.fillRect({cx - 4, hy + 3, 8, 8}, lv.handleInner);
    r.hLine(cx - 5, hy + 7, 11, c.ink);
}

void paintLamp(PixelRenderer& r, int cx, int cy, Color lit, bool on, const RetroConfig& c) {
    r.fillCircle({static_cast<float>(cx) + 0.5f, static_cast<float>(cy) + 0.5f}, 3.0f, c.ledEdge);
    r.fillCircle({static_cast<float>(cx) + 0.5f, static_cast<float>(cy) + 0.5f}, 2.0f, on ? lit : shade(lit, 0.25f));
    if (on)
        r.pixel(cx - 1, cy - 1, 0xC0FFFFFF);
}

} // namespace

void RetroToggle::paint(IRenderer& rr, const Rect& b, const ToggleState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r)
        return;
    const RetroConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    const int cap = capOf(c.font);
    r->pushClip(b);
    if (c.toggle == ToggleShape::Lever) {
        const int cx = b.x + b.w / 2;
        // The label above the plate, when the bounds are taller than both.
        const bool label = s.label && *s.label && b.h >= 44 + cap + 4;
        const int top = label ? b.y + cap + 4 : b.y;
        const int cy = top + (b.bottom() - top) / 2;
        if (label)
            putCentered(*r, c.font, s.label, cx, b.y + 2, c.ink);
        paintLever(*r, cx, cy, s, c);
        if (c.lever.lamp)
            paintLamp(*r, cx + 15, cy, hasOwnColor(theme, s.accent) ? s.accent : 0xFFFF6A00u, s.on, c);
    } else {
        // A bevelled key with a lamp, the label on the key.
        const int kh = std::clamp(cap + 6, 7, std::max(7, b.h - 2));
        const Rect key{b.x + 1, b.y + (b.h - kh) / 2, std::max(1, b.w - 2), kh};
        const Color lit = hasOwnColor(theme, s.accent) ? s.accent : c.inkOn;
        paintFace(*r, key, c.button.face, s.on, s.active, s.hot, c);
        const Rect inner = key.inset(1);
        r->fillRect({inner.x + 3, inner.y + inner.h / 2 - 1, 3, 3}, s.on ? lit : shade(c.button.dark, 0.7f));
        if (s.label && *s.label) {
            r->pushClip(inner);
            putCenteredInRow(*r, c.font, s.label, inner.x + 6 + (inner.w - 6) / 2, inner,
                             s.on ? c.button.inkOn : c.button.ink);
            r->popClip();
        }
    }
    if (s.focused)
        focusRect(*r, b, theme);
    r->popClip();
}

// --- selector ------------------------------------------------------------------------------------------------------

Rect RetroSelector::cell(const Rect& b, const SelectorState& s, int i) const {
    const int n = s.count > 0 ? s.count : 1;
    if (s.vertical) {
        // The lever's positions sit between 4 pixels of margin at the ends.
        const int span = b.h - 8;
        const int y0 = b.y + 4 + span * i / n, y1 = b.y + 4 + span * (i + 1) / n;
        return {b.x, y0, b.w, y1 - y0};
    }
    return SelectorPainter::cell(b, s, i);
}

void RetroSelector::paint(IRenderer& rr, const Rect& b, const SelectorState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r)
        return;
    const RetroConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    const int n = std::max(0, s.count);
    r->pushClip(b);
    if (s.vertical) {
        // A lever in a slot at the left, the step names to its right; the selected position holds the lever.
        const int slotX = b.x + 7;
        const Rect slot{slotX - 2, b.y + 2, 5, b.h - 4};
        r->fillRect(slot, c.slider.slotEdge);
        r->fillRect(slot.inset(1), c.slider.slot);
        const Color capColor = hasOwnColor(theme, s.accent) ? s.accent : c.button.face;
        const int span = b.h - 8;
        for (int i = 0; i < n; ++i) {
            const int y = b.y + 4 + span * (2 * i + 1) / (2 * std::max(1, n));
            const bool sel = i == s.index;
            const Color tint = sel ? c.inkOn : (i == s.hotIndex ? c.ink : c.inkDim);
            r->hLine(slotX + 3, y, 3, c.inkDim);
            if (s.icons && s.icons[i]) {
                s.icons[i](*r, {slotX + 9, y - 4, 16, 8}, tint, 1.0f);
            } else {
                const std::string label = stepLabel(s, i);
                put(*r, c.font, label.c_str(), slotX + 9, y - capOf(c.font) / 2, tint);
            }
            if (sel)
                paintFace(*r, {slotX - 5, y - 3, 11, 7}, capColor, false, false, false, c);
        }
    } else {
        for (int i = 0; i < n; ++i) {
            const Rect cl = cell(b, s, i).inset(1);
            const bool sel = i == s.index;
            paintFace(*r, cl, c.button.face, sel, false, i == s.hotIndex, c);
            const Color tint = sel ? c.button.inkOn : c.button.ink;
            const Rect inner = cl.inset(1);
            r->pushClip(inner);
            if (s.icons && s.icons[i]) {
                const int ih = std::clamp(inner.h - 4, 4, 12);
                const int iw = std::min(ih * 2, std::max(4, inner.w - 4));
                s.icons[i](*r, {inner.x + (inner.w - iw) / 2, inner.y + (inner.h - ih) / 2, iw, ih}, tint, 1.0f);
            } else {
                const std::string label = stepLabel(s, i);
                putCenteredInRow(*r, c.font, label.c_str(), cl.x + cl.w / 2, cl, tint);
            }
            r->popClip();
        }
    }
    if (s.focused)
        focusRect(*r, b, theme);
    r->popClip();
}

// --- button --------------------------------------------------------------------------------------------------------

void RetroButton::paint(IRenderer& rr, const Rect& b, const ButtonState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r)
        return;
    const RetroConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    const int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
    r->pushClip(b);
    switch (s.variant) {
    case kButtonCap: {
        // A colored cap, dim when off, lit when on, with a one- or two-line legend.
        const Color base = s.accent ? s.accent : theme.colors.accent;
        paintFace(*r, b, s.on ? lift(base, 0.1f) : shade(base, 0.55f), false, s.active, s.hot, c);
        const Color legend = s.on ? 0xF0101010u : 0xC0101010u;
        const char* label = s.label ? s.label : "";
        const char* nl = std::strchr(label, '\n');
        const int cap = capOf(c.font);
        r->pushClip(b.inset(2));
        if (nl && b.h >= 2 * cap + 6) {
            const std::string top(label, static_cast<std::size_t>(nl - label));
            const int y0 = b.y + (b.h - (2 * cap + 2)) / 2;
            putCentered(*r, c.font, top.c_str(), cx, y0, legend);
            putCentered(*r, c.font, nl + 1, cx, y0 + cap + 2, legend);
        } else {
            const std::string first = nl ? std::string(label, static_cast<std::size_t>(nl - label)) : label;
            putCenteredInRow(*r, c.font, first.c_str(), cx, b, legend);
        }
        r->popClip();
        break;
    }
    case kButtonDisplay: {
        // A recessed display of the label with a chevron, lit at its edge while pressed.
        paintDisplay(*r, b, s.hot, s.active ? theme.colors.focus : 0u, c);
        r->pushClip(b.inset(1));
        const std::string text = s.label ? s.label : "";
        putCenteredInRow(*r, c.font, text.c_str(), cx - 4, b, c.display.ink);
        triangleDown(*r, b.right() - 8, cy - 1, 5, c.display.inkDim);
        r->popClip();
        break;
    }
    default: {
        paintFace(*r, b, c.button.face, s.on, s.active, s.hot, c);
        const Color ink = s.on ? c.button.inkOn : c.button.ink;
        if (s.variant == kButtonArrowLeft || s.variant == kButtonArrowRight) {
            chevron(*r, cx, cy, s.variant == kButtonArrowLeft ? -1 : 1, ink);
            break;
        }
        const bool text = s.label && *s.label;
        int textX = cx;
        r->pushClip(b.inset(2));
        if (s.icon) {
            // The icon alone is centered; beside a label it takes the left end.
            const int ih = std::clamp(b.h - 8, 4, 12);
            const int iw = std::min(ih * 2, std::max(4, b.w - 8));
            const int ix = text ? b.x + 4 : cx - iw / 2;
            s.icon(*r, {ix, cy - ih / 2, iw, ih}, ink, 1.0f);
            textX = b.x + 4 + iw + (b.w - 4 - iw) / 2;
        }
        if (text)
            putCenteredInRow(*r, c.font, s.label, textX, b, ink);
        r->popClip();
        break;
    }
    }
    if (s.focused)
        focusRect(*r, b, theme);
    r->popClip();
}

// --- panel ---------------------------------------------------------------------------------------------------------

namespace {

void rivet(PixelRenderer& r, int cx, int cy) {
    const Point p{static_cast<float>(cx) + 0.5f, static_cast<float>(cy) + 0.5f};
    r.fillCircle(p, 3.0f, 0xFF121416);
    r.strokeCircle(p, 4.0f, 0xFF3A3D40, 1.0f);
    r.fillCircle(p, 2.0f, 0xFF282A2C);
    r.pixel(cx - 1, cy - 1, 0xFF888C90);
}

// A forged plate: a border, a gradient, a trim line inside the edge, rivets and the title on a plate.
void paintPlate(PixelRenderer& r, const Rect& b, const char* title, const RetroConfig& c) {
    const RetroConfig::Frame& f = c.frame;
    r.fillRect(b, f.border);
    r.fillGradientRect(b.inset(2), f.fillTop, f.fillBottom);
    r.strokeRect(b.inset(3), f.trim);
    r.strokeRect(b.inset(4), f.trimInner);
    if (f.rivets) {
        rivet(r, b.x + 8, b.y + 8);
        rivet(r, b.right() - 9, b.y + 8);
        rivet(r, b.x + 8, b.bottom() - 9);
        rivet(r, b.right() - 9, b.bottom() - 9);
    }
    const BitmapFont* font = titleFontOf(c);
    if (title && *title && font) {
        const int tw = widthOf(font, title) + 12;
        const Rect plate{b.x + (b.w - tw) / 2, b.y, tw, 14};
        r.fillRect(plate, f.titleFill);
        r.strokeRect(plate, f.trim);
        putInRow(r, font, title, plate.x + 6, plate, orElse(f.titleInk, c.ink));
    }
}

} // namespace

void RetroPanel::paint(IRenderer& rr, const Rect& b, const PanelState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r)
        return;
    const RetroConfig& c = cfg();
    r->pushClip(b);
    const bool titled = s.title && *s.title;
    if (!titled) {
        // The surface, and the trims along its top and bottom edges.
        r->fillRect(b, c.surface);
        if (c.trims.on) {
            const RetroConfig::Trims& t = c.trims;
            r->strokeRect({b.x, b.y, b.w, t.top}, t.line);
            r->hLine(b.x, b.y + t.top, b.w, t.shadow);
            r->hLine(b.x, b.y + t.top + 1, b.w, t.light);
            r->hLine(b.x, b.bottom() - t.bottom - 1, b.w, t.shadow);
            r->strokeRect({b.x, b.bottom() - t.bottom, b.w, t.bottom}, t.line);
        }
    } else if (c.frame.plate) {
        paintPlate(*r, b, s.title, c);
    } else {
        // A frame with the title cut into its top line.
        const BitmapFont* font = titleFontOf(c);
        const int cap = capOf(font);
        const int y0 = b.y + cap / 2 + 1;
        const Rect box{b.x, y0, b.w - (c.frame.etched ? 1 : 0), b.h - (y0 - b.y) - (c.frame.etched ? 1 : 0)};
        if (c.frame.etched)
            r->strokeRect({box.x + 1, box.y + 1, box.w, box.h}, c.trims.light);
        r->strokeRect(box, c.frameInk);
        const int tw = widthOf(font, s.title) + 8;
        const int tx = b.x + (b.w - tw) / 2;
        r->fillRect({tx, y0, tw, c.frame.etched ? 2 : 1}, c.surface);
        putCentered(*r, font, s.title, b.x + b.w / 2, b.y + 1, c.ink);
    }
    r->popClip();
}

// --- lamp ------------------------------------------------------------------------------------------------------------

void RetroLed::paint(IRenderer& rr, const Rect& b, const LedState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r)
        return;
    const RetroConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    const int radius = std::clamp(b.h / 2 - 2, 3, 5);
    const int cy = b.y + b.h / 2, cx = b.x + radius + 1;
    r->pushClip(b);
    const Color accent = s.accent ? s.accent : theme.colors.accent;
    const Point p{static_cast<float>(cx) + 0.5f, static_cast<float>(cy) + 0.5f};
    const Color dark = shade(accent, 0.25f);
    r->fillCircle(p, static_cast<float>(radius), c.ledEdge);
    r->fillCircle(p, static_cast<float>(radius - 1),
                  s.on ? ui::mix(dark, accent, std::clamp(s.level, 0.0f, 1.0f)) : dark);
    if (s.on && s.level > 0.5f)
        r->pixel(cx - radius / 2, cy - radius / 2, 0xC0FFFFFF);
    if (s.label && *s.label)
        putInRow(*r, c.font, s.label, cx + radius + 4, b, s.on ? c.ink : c.inkDim);
    r->popClip();
}

// --- keyboard --------------------------------------------------------------------------------------------------------

void RetroKeyboard::paint(IRenderer& rr, const Rect& b, const KeyboardState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r || b.empty())
        return;
    const RetroConfig& c = cfg();
    const RetroConfig::Keyboard& kb = c.keyboard;
    const Theme& theme = themeOf(s.theme, c);
    const keyboard::Layout lay = layout();
    const Color accent = s.accent ? s.accent : theme.colors.accent;
    r->pushClip(b);
    // The felt strip behind the keys, then the white keys, then the black ones over them.
    r->fillRect({b.x, b.y, b.w, std::min(3, b.h)}, kb.felt);
    auto edgeOf = [](float x) { return static_cast<int>(std::floor(x + 0.5f + 1e-3f)); };
    for (int k = s.first; k <= s.last; ++k) {
        if (keyboard::isBlack(k))
            continue;
        const keyboard::KeyRect kr = keyboard::keyRect(b, s.first, s.last, k, lay);
        const bool down = s.down && s.down[k], sounding = s.lit && s.lit[k];
        const bool hot = s.hotKey == k && !down;
        Color col = down ? kb.whiteDown : kb.white;
        if (sounding)
            col = ui::mix(col, accent, 0.45f);
        if (hot)
            col = shade(col, 0.96f);
        const int x0 = edgeOf(kr.x0), x1 = edgeOf(kr.x1);
        const Rect key{x0, b.y + 3, std::max(1, x1 - x0 - 1), b.h - 3};
        r->fillRect(key, col);
        r->vLine(x1 - 1, key.y, key.h, kb.edge);
        r->hLine(key.x, b.bottom() - 2, key.w, shade(col, down ? 0.7f : 0.82f));
    }
    for (int k = s.first; k <= s.last; ++k) {
        if (!keyboard::isBlack(k))
            continue;
        const keyboard::KeyRect kr = keyboard::keyRect(b, s.first, s.last, k, lay);
        const bool down = s.down && s.down[k], sounding = s.lit && s.lit[k];
        Color col = down ? kb.blackDown : kb.black;
        if (sounding)
            col = ui::mix(col, shade(accent, 0.6f), 0.6f);
        const int x0 = edgeOf(kr.x0), x1 = edgeOf(kr.x1), y1 = edgeOf(kr.y1);
        const Rect key{x0, b.y + 3, std::max(1, x1 - x0), y1 - b.y - 3};
        r->fillRect(key, col);
        r->vLine(key.x, key.y, key.h - 1, lift(col, 0.25f));
        r->hLine(key.x, key.bottom() - 1, key.w, lift(col, 0.12f));
    }
    r->popClip();
}

// --- display widgets -------------------------------------------------------------------------------------------------

void RetroDropdown::paint(IRenderer& rr, const Rect& b, const DropdownState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r)
        return;
    const RetroConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    r->pushClip(b);
    paintDisplay(*r, b, s.hot && !s.open, s.open ? theme.colors.focus : 0u, c);
    r->pushClip(b.inset(1));
    int tx = b.x + 5;
    if (s.label && *s.label) {
        putInRow(*r, c.font, s.label, tx, b, c.display.inkDim);
        tx += widthOf(c.font, s.label) + 6;
    }
    putInRow(*r, c.font, s.valueText ? s.valueText : "", tx, b, c.display.ink);
    r->popClip();
    triangleDown(*r, b.right() - 8, b.y + b.h / 2 - 1, 5, c.display.inkDim);
    if (s.focused)
        focusRect(*r, b, theme);
    r->popClip();
}

void RetroStepper::paint(IRenderer& rr, const Rect& b, const StepperState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r)
        return;
    const RetroConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    r->pushClip(b);
    paintDisplay(*r, b, false, 0u, c);
    for (int k = 0; k < 2; ++k) {
        const Rect part = (k == 0 ? minusPart(b) : plusPart(b)).inset(1);
        const bool hot = s.hotPart == (k == 0 ? -1 : 1);
        paintFace(*r, part, c.button.face, false, false, hot, c);
        const Point mid = part.center();
        const int mx = static_cast<int>(mid.x), my = static_cast<int>(mid.y);
        r->hLine(mx - 3, my, 7, c.button.ink);
        if (k == 1)
            r->vLine(mx, my - 3, 7, c.button.ink);
    }
    std::string text = s.valueText ? s.valueText : "";
    if (s.label && *s.label)
        text = std::string(s.label) + " " + text;
    const Rect lo = minusPart(b), hi = plusPart(b);
    r->pushClip({lo.right(), b.y, hi.x - lo.right(), b.h});
    putCenteredInRow(*r, c.font, text.c_str(), lo.right() + (hi.x - lo.right()) / 2, b, c.display.ink);
    r->popClip();
    if (s.focused)
        focusRect(*r, b, theme);
    r->popClip();
}

void RetroTextField::paint(IRenderer& rr, const Rect& b, const TextFieldState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r)
        return;
    const RetroConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    r->pushClip(b);
    paintDisplay(*r, b, false, theme.colors.focus, c);
    r->pushClip(b.inset(1));
    const std::string shown = std::string(s.text ? s.text : "") + (s.caret ? "_" : " ");
    putInRow(*r, c.font, shown.c_str(), b.x + 5, b, c.display.ink);
    r->popClip();
    r->popClip();
}

// --- popup and tooltip -----------------------------------------------------------------------------------------------

namespace {

constexpr int kPopupPad = 2, kSeparatorH = 5;

void paintFloating(PixelRenderer& r, const Rect& b, Color fill, Color edge) {
    r.fillRect({b.x + 2, b.y + 2, b.w, b.h}, 0x50000000); // the shadow
    r.fillRect(b, fill);
    r.strokeRect(b, edge);
}

} // namespace

Rect RetroPopup::layout(IRenderer& /*r*/, const PopupState& s, int viewW, int viewH, std::vector<Rect>& rects) const {
    const RetroConfig& c = cfg();
    const Theme::Metrics& m = s.theme->metrics;
    const std::vector<PopupItem>& items = *s.items;
    const int n = static_cast<int>(items.size());
    const int cols = std::max(1, s.columns);
    const int rows = std::max(1, (n + cols - 1) / cols);
    int itemW = s.anchor.w;
    for (const PopupItem& it : items)
        if (!it.separator)
            itemW = std::max(itemW, widthOf(c.font, it.label.c_str()) + 20);
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
    int x = s.anchor.x, y = s.anchor.empty() ? s.anchor.y : s.anchor.bottom() + 1;
    if (y + h > viewH - 4)
        y = std::max(4, s.anchor.y - h - (s.anchor.empty() ? 0 : 1));
    if (x + w > viewW - 4)
        x = std::max(4, viewW - 4 - w);
    for (Rect& it : rects) {
        it.x += x;
        it.y += y;
    }
    return {x, y, w, h};
}

void RetroPopup::paint(IRenderer& rr, const Rect& b, const PopupState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r)
        return;
    const RetroConfig& c = cfg();
    const RetroConfig::Popup& p = c.popup;
    r->pushClip(clipOf(b, margin()));
    paintFloating(*r, b, p.fill, p.edge);
    const std::vector<PopupItem>& items = *s.items;
    for (std::size_t i = 0; i < items.size() && s.itemRects && i < s.itemRects->size(); ++i) {
        const Rect& cell = (*s.itemRects)[i];
        const PopupItem& it = items[i];
        if (it.separator) {
            r->hLine(cell.x + 3, cell.y + cell.h / 2, cell.w - 6, p.edge);
            continue;
        }
        const bool hot = static_cast<int>(i) == s.hot && it.enabled;
        if (hot)
            r->fillRect(cell, p.hot);
        if (static_cast<int>(i) == s.selected)
            r->fillRect({cell.x + 4, cell.y + cell.h / 2 - 1, 3, 3}, hot ? p.hotInk : p.ink);
        r->pushClip(cell);
        putInRow(*r, c.font, it.label.c_str(), cell.x + 10, cell, !it.enabled ? p.inkDim : (hot ? p.hotInk : p.ink));
        r->popClip();
    }
    r->popClip();
}

Rect RetroTooltip::layout(IRenderer& /*r*/, const TooltipState& s, int viewW, int viewH) const {
    const RetroConfig& c = cfg();
    const bool body = s.text && *s.text;
    const int cap = capOf(c.font);
    const int w = std::max(widthOf(c.font, s.title ? s.title : ""), body ? widthOf(c.font, s.text) : 0) + 8;
    const int iw = std::min(w, std::max(0, viewW - 8)), ih = body ? 2 * cap + 10 : cap + 8;
    int x = static_cast<int>(s.pointer.x) + 10, y = static_cast<int>(s.pointer.y) + 14;
    if (x + iw > viewW - 4)
        x = std::max(4, viewW - 4 - iw);
    if (y + ih > viewH - 4)
        y = std::max(4, static_cast<int>(s.pointer.y) - ih - 8);
    return {x, y, iw, ih};
}

void RetroTooltip::paint(IRenderer& rr, const Rect& b, const TooltipState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r)
        return;
    const RetroConfig& c = cfg();
    const RetroConfig::Tooltip& t = c.tooltip;
    r->pushClip(clipOf(b, margin()));
    paintFloating(*r, b, t.fill, t.edge);
    r->pushClip(b.inset(2, 0));
    const bool body = s.text && *s.text;
    const int cap = capOf(c.font);
    put(*r, c.font, s.title ? s.title : "", b.x + 4, b.y + 4, t.ink);
    if (body)
        put(*r, c.font, s.text, b.x + 4, b.y + 4 + cap + 2, t.inkDim);
    r->popClip();
    r->popClip();
}

// --- tabs and readout
// --------------------------------------------------------------------------------------------------

void RetroTabs::paint(IRenderer& rr, const Rect& b, const TabsState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r)
        return;
    const RetroConfig& c = cfg();
    const Theme& theme = themeOf(s.theme, c);
    r->pushClip(b);
    for (int i = 0; i < s.count; ++i) {
        const Rect cl = cell(b, s, i).inset(1);
        const bool on = i == s.index;
        paintFace(*r, cl, c.button.face, on, false, i == s.hotIndex, c);
        const char* text = s.labels && s.labels[i] ? s.labels[i] : "";
        r->pushClip(cl.inset(1));
        putCenteredInRow(*r, c.font, text, cl.x + cl.w / 2, cl, on ? c.button.inkOn : c.button.ink);
        r->popClip();
    }
    if (s.focused)
        focusRect(*r, b, theme);
    r->popClip();
}

void RetroReadout::paint(IRenderer& rr, const Rect& b, const ReadoutState& s) const {
    PixelRenderer* r = asPixel(rr);
    if (!r)
        return;
    const RetroConfig& c = cfg();
    r->pushClip(b);
    paintDisplay(*r, b, false, 0u, c);
    r->pushClip(b.inset(1));
    const int x = b.x + 5;
    if (s.idle) {
        putInRow(*r, c.font, s.name, x, b, c.display.inkDim);
    } else {
        const std::string line = std::string(s.name) + ": " + s.value;
        putInRow(*r, c.font, line.c_str(), x, b, c.display.ink);
        if (s.hint && *s.hint) {
            // The hint, dimmer, at the right when it fits.
            const int lw = widthOf(c.font, line.c_str()), hw = widthOf(c.font, s.hint);
            const int hx = b.right() - 6 - hw;
            if (hx > x + lw + 8)
                putInRow(*r, c.font, s.hint, hx, b, c.display.inkDim);
        }
    }
    r->popClip();
    r->popClip();
}

} // namespace spux::inline v1::ui::retro
