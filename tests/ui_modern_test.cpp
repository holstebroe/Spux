// spux_ui_modern on the software renderer: the renderer's primitives, every painter inside its bounds and
// margin, the style configuration, the interaction geometry, the showcase page as golden frames, and the
// editor shell's cached frames against uncached ones.
#include <doctest.h>

#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "spux/ui/Editor.hpp"
#include "spux/ui/modern/FontAtlas.hpp"
#include "spux/ui/modern/ModernSurface.hpp"
#include "spux/ui/modern/SoftRenderer.hpp"
#include "ui/ModernSupport.hpp"

using namespace modern_test;

namespace {

std::uint32_t at(const std::vector<std::uint32_t>& px, int w, int x, int y) {
    return px[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)];
}

int diffCount(const std::vector<std::uint32_t>& a, const std::vector<std::uint32_t>& b) {
    if (a.size() != b.size())
        return -1;
    int n = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        n += a[i] != b[i];
    return n;
}

} // namespace

TEST_CASE("soft renderer: fills, clip, alpha and additive blend") {
    SoftRenderer r = makeSoft(64, 48, 2);
    r.clear(rgb(0, 0, 0));
    r.fillRect({10, 10, 20, 20}, rgb(255, 0, 0));
    r.pushClip({0, 0, 15, 15});
    r.fillRect({0, 0, 64, 48}, rgb(0, 255, 0));
    r.popClip();
    auto px = r.frame();
    CHECK(at(px, 64, 12, 12) == rgb(0, 255, 0));
    CHECK(at(px, 64, 20, 20) == rgb(255, 0, 0));
    CHECK(at(px, 64, 40, 40) == rgb(0, 0, 0));

    r.clear(rgb(0, 0, 0));
    r.fillRect({0, 0, 8, 8}, rgb(200, 100, 50, 128));
    CHECK(((at(r.frame(), 64, 4, 4) >> 16) & 255u) == doctest::Approx(100).epsilon(0.03));

    r.clear(rgb(100, 100, 100));
    r.setBlend(Blend::Add);
    r.fillRect({0, 0, 8, 8}, rgb(100, 50, 200, 255));
    r.setBlend(Blend::Alpha);
    px = r.frame();
    CHECK(((at(px, 64, 4, 4) >> 16) & 255u) == 200u);
    CHECK((at(px, 64, 4, 4) & 255u) == 255u); // clamped
}

TEST_CASE("soft renderer: shapes have soft edges, the right area, at every quality") {
    for (int ss = 1; ss <= 4; ++ss) {
        SoftRenderer r = makeSoft(40, 40, ss);
        CHECK(r.supersample() == ss);
        r.clear(rgb(0, 0, 0));
        r.fillCircle({20.0f, 20.0f}, 10.0f, rgb(255, 255, 255));
        const auto px = r.frame();
        double sum = 0;
        bool partial = false;
        for (std::uint32_t p : px) {
            const double v = static_cast<double>(p & 255u) / 255.0;
            sum += v;
            partial = partial || (v > 0.05 && v < 0.95);
        }
        INFO("supersample " << ss);
        CHECK(partial);
        CHECK(sum == doctest::Approx(3.14159 * 100.0).epsilon(0.03));
    }
}

TEST_CASE("soft renderer: arcs, strips, glow, gradients and rounded boxes stay inside the buffer") {
    SoftRenderer r = makeSoft(120, 80, 2);
    r.clear(rgb(10, 10, 10));
    r.drawArc({30.0f, 30.0f}, 20.0f, 2.1f, 7.3f, rgb(255, 200, 0), 6.0f);
    const auto px = r.frame();
    CHECK(luma(at(px, 120, 30, 30)) < 15);  // the arc leaves the center alone
    CHECK(luma(at(px, 120, 50, 30)) > 150); // on the ring, at 0 radians
    const Point line[4] = {{-20.0f, -20.0f}, {40.0f, 60.0f}, {90.0f, 20.0f}, {200.0f, 100.0f}};
    const Color cols[4] = {rgb(255, 0, 0), rgb(0, 255, 0), rgb(0, 0, 255), rgb(255, 255, 255)};
    const float ws[4] = {1.0f, 3.0f, 5.0f, 2.0f};
    r.drawStrip(line, cols, ws, 4);
    r.drawGlow({60.0f, 40.0f}, 30.0f, rgb(80, 160, 255));
    r.fillRadialGradient({90.0f, 60.0f}, 15.0f, rgb(255, 255, 255), rgb(0, 0, 0));
    r.drawPanel({5, 50, 40, 25}, 8.0f, rgb(12, 16, 30, 148));
    r.fillGradientRect({0, 0, 120, 6}, rgb(0, 0, 0), rgb(255, 255, 255));
    const auto after = r.frame();
    CHECK(after.size() == 120u * 80u);
    CHECK(luma(at(after, 120, 60, 40)) > 40); // the glow
}

TEST_CASE("soft renderer: translucent polylines do not darken at joints") {
    SoftRenderer r = makeSoft(80, 40, 2);
    r.clear(rgb(0, 0, 0));
    Point pts[40];
    for (int i = 0; i < 40; ++i)
        pts[i] = {4.0f + static_cast<float>(i) * 1.8f, 20.0f};
    r.drawPolyline(pts, 40, rgb(255, 255, 255, 128), 3.0f);
    const auto px = r.frame();
    int lo = 255, hi = 0;
    for (int x = 8; x < 70; ++x) {
        const int v = luma(at(px, 80, x, 20));
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    CHECK(hi - lo <= 2);
    CHECK(hi == doctest::Approx(128).epsilon(0.03));
}

TEST_CASE("soft renderer: text is the atlas font, sized by pixels, aligned, and matches its metrics") {
    SoftRenderer r = makeSoft(240, 80, 2);
    r.clear(rgb(0, 0, 0));
    r.drawTextPx("Cutoff 1.2 kHz", 10, 10, rgb(255, 255, 255), 16.0f);
    r.drawTextPx("Right", 230, 40, rgb(255, 255, 255), 12.0f, Align::Right, FontWeight::Bold);
    const auto px = r.frame();
    int lit = 0, litRight = 0;
    for (int y = 0; y < 80; ++y)
        for (int x = 0; x < 240; ++x) {
            const bool on = luma(at(px, 240, x, y)) > 100;
            if (y < 30)
                lit += on;
            else if (y >= 38)
                litRight += on;
            if (y >= 38 && on)
                CHECK(x < 231);
        }
    CHECK(lit > 150);
    CHECK(litRight > 40);
    FontAtlas atlas;
    REQUIRE(atlas.build());
    CHECK(r.textWidthPx("Cutoff 1.2 kHz", 16.0f) ==
          doctest::Approx(atlas.width(FontWeight::Regular, "Cutoff 1.2 kHz") * 16.0f));
    // tabular digits: changing a number does not move its neighbors
    CHECK(r.textWidthPx("1111", 12.0f) == doctest::Approx(r.textWidthPx("0880", 12.0f)));
    CHECK(r.textWidthPx("\xE2\x80\xA6", 12.0f) > 0.0f); // the ellipsis is in the atlas
}

TEST_CASE("soft renderer: the layers behave as the editor expects") {
    SoftRenderer r = makeSoft(50, 50, 2);
    REQUIRE(r.layers() != nullptr);
    CHECK_FALSE(r.isGpu());
    r.cpu().clearDirty();
    r.cpu().markDirty({10, 10, 5, 5});
    CHECK(r.cpu().dirtyBounds() == Rect{10, 10, 5, 5});
}

namespace {
struct PaintCase {
    std::string name;
    Margin margin;
    std::function<void(IRenderer&, const Rect&)> paint;
};
} // namespace

TEST_CASE("every painter draws inside its bounds plus its margin") {
    ModernStyle style;
    const Theme* th = &style.theme;
    const params::ParamInfo* info = &kInfos[0]; // quality: Enum
    const char* labels[3] = {"Aaaaaaaaaaaaaaaa", "Bbbbbbbbbbbbbbbbbbb", "C"};

    std::vector<PaintCase> cases;
    for (int large = 0; large < 2; ++large)
        for (int variant = 0; variant < 6; ++variant) {
            const std::string tag =
                std::string("knob large=") + std::to_string(large) + " v=" + std::to_string(variant);
            cases.push_back({tag, style.knob.margin(), [=, &style](IRenderer& r, const Rect& b) {
                                 KnobState s;
                                 s.theme = th;
                                 s.large = large != 0;
                                 s.label = "A VERY LONG LABEL THAT CANNOT FIT";
                                 s.valueText = "12345.678 kHz and more";
                                 s.accent = rgb(255, 186, 84);
                                 s.value = variant == 1 ? 0.0 : variant == 2 ? 1.0 : 0.63;
                                 s.hot = variant == 3;
                                 s.focused = variant == 3 || variant == 4;
                                 s.bipolar = variant == 4;
                                 s.dimmed = variant == 5;
                                 s.editing = variant == 5;
                                 s.editText = "-12.5 dB entered long text";
                                 s.caret = true;
                                 s.mod.any = variant >= 3;
                                 s.mod.armed = variant >= 4;
                                 s.mod.armedRoute = variant == 5;
                                 s.mod.armedDepth = 0.3;
                                 s.mod.base = 0.5;
                                 s.mod.shared = 0.8;
                                 s.mod.voiceCount = 4;
                                 for (int v = 0; v < 4; ++v)
                                     s.mod.voices[v] = 0.1 + 0.2 * v;
                                 style.knob.paint(r, b, s);
                             }});
        }
    for (int vertical = 0; vertical < 2; ++vertical)
        for (int v = 0; v < 3; ++v)
            cases.push_back({vertical ? "slider vertical" : "slider horizontal", style.slider.margin(),
                             [=, &style](IRenderer& r, const Rect& b) {
                                 SliderState s;
                                 s.theme = th;
                                 s.vertical = vertical != 0;
                                 s.value = v == 0 ? 0.0 : v == 1 ? 0.5 : 1.0;
                                 s.label = v == 2 ? "" : "A long label for the slider";
                                 s.valueText = v == 1 ? "" : "+12.34 dB value";
                                 s.accent = rgb(112, 170, 255);
                                 s.focused = v == 1;
                                 style.slider.paint(r, b, s);
                             }});
    for (int on = 0; on < 2; ++on)
        cases.push_back({"toggle", style.toggle.margin(), [=, &style](IRenderer& r, const Rect& b) {
                             ToggleState s;
                             s.theme = th;
                             s.on = on != 0;
                             s.label = "A toggle label that is too long to fit";
                             s.accent = rgb(118, 226, 164);
                             s.focused = true;
                             style.toggle.paint(r, b, s);
                         }});
    for (int vertical = 0; vertical < 2; ++vertical)
        cases.push_back({"selector", style.selector.margin(), [=, &style](IRenderer& r, const Rect& b) {
                             SelectorState s;
                             s.theme = th;
                             s.count = 3;
                             s.index = 1;
                             s.hotIndex = 2;
                             s.labels = labels;
                             s.vertical = vertical != 0;
                             s.accent = rgb(112, 170, 255);
                             s.focused = true;
                             style.selector.paint(r, b, s);
                             s.labels = nullptr;
                             s.param = info;
                             style.selector.paint(r, b, s);
                         }});
    for (int variant = 0; variant < 2; ++variant)
        for (int on = 0; on < 2; ++on)
            cases.push_back({"button", style.button.margin(), [=, &style](IRenderer& r, const Rect& b) {
                                 ButtonState s;
                                 s.theme = th;
                                 s.variant = variant;
                                 s.on = on != 0;
                                 s.hot = true;
                                 s.focused = true;
                                 s.label = "A BUTTON WITH A LONG LABEL";
                                 s.accent = rgb(255, 186, 84);
                                 style.button.paint(r, b, s);
                             }});
    for (int on = 0; on < 2; ++on)
        cases.push_back({"led", style.led.margin(), [=, &style](IRenderer& r, const Rect& b) {
                             LedState s;
                             s.theme = th;
                             s.on = on != 0;
                             s.level = on ? 0.6f : 1.0f;
                             s.label = "A LAMP WITH A LONG LABEL";
                             s.accent = rgb(214, 58, 46);
                             style.led.paint(r, b, s);
                         }});
    for (int v = 0; v < 3; ++v)
        cases.push_back({"keyboard", style.keyboard.margin(), [=, &style](IRenderer& r, const Rect& b) {
                             std::uint8_t down[128] = {}, sounding[128] = {};
                             down[48] = 1;
                             sounding[52] = sounding[v == 2 ? 70 : 66] = 1;
                             KeyboardState s;
                             s.theme = th;
                             s.first = v == 2 ? 47 : 48;
                             s.last = v == 2 ? 70 : 71;
                             s.down = down;
                             s.lit = sounding;
                             s.hotKey = v == 0 ? 50 : -1;
                             s.accent = rgb(255, 186, 84);
                             style.keyboard.paint(r, b, s);
                         }});
    cases.push_back({"icons", style.button.margin(), [=, &style](IRenderer& r, const Rect& b) {
                         ButtonState s;
                         s.theme = th;
                         s.icon = waveIcon(Wave::Saw);
                         s.label = "SAW WITH A LONG LABEL";
                         style.button.paint(r, b, s);
                         s.label = "";
                         style.button.paint(r, b, s);
                         const Icon icons[3] = {waveIcon(Wave::Sine), waveIcon(Wave::Pulse), waveIcon(Wave::Noise)};
                         SelectorState se;
                         se.theme = th;
                         se.count = 3;
                         se.index = 1;
                         se.icons = icons;
                         style.selector.paint(r, b, se);
                         se.vertical = true;
                         style.selector.paint(r, b, se);
                     }});
    cases.push_back({"tabs", style.tabs.margin(), [=, &style](IRenderer& r, const Rect& b) {
                         static const char* const names[3] = {"LFO 1", "A VERY LONG TAB NAME", "MOD"};
                         TabsState s;
                         s.theme = th;
                         s.count = 3;
                         s.index = 1;
                         s.hotIndex = 2;
                         s.labels = names;
                         s.focused = true;
                         s.accent = rgb(255, 186, 84);
                         style.tabs.paint(r, b, s);
                     }});
    for (int idle = 0; idle < 2; ++idle)
        cases.push_back({"readout", style.readout.margin(), [=, &style](IRenderer& r, const Rect& b) {
                             ReadoutState s;
                             s.theme = th;
                             s.idle = idle != 0;
                             s.name = idle ? "TOUCH A CONTROL TO SEE ITS VALUE HERE" : "A LONG PARAMETER NAME";
                             s.value = "-12.5 dB and then some";
                             s.hint = "Drag up or down; shift for fine";
                             style.readout.paint(r, b, s);
                         }});
    for (int v : {kButtonArrowLeft, kButtonArrowRight, kButtonDisplay})
        cases.push_back(
            {"button v=" + std::to_string(v), style.button.margin(), [=, &style](IRenderer& r, const Rect& b) {
                 ButtonState s;
                 s.theme = th;
                 s.variant = v;
                 s.hot = true;
                 s.active = true;
                 s.focused = true;
                 s.label = "A PRESET NAME THAT IS FAR TOO LONG";
                 s.accent = rgb(255, 186, 84);
                 style.button.paint(r, b, s);
             }});
    cases.push_back({"pickable knob", style.knob.margin(), [=, &style](IRenderer& r, const Rect& b) {
                         KnobState s;
                         s.theme = th;
                         s.pickable = true;
                         s.value = 0.5;
                         s.label = "CUTOFF";
                         s.accent = rgb(255, 186, 84);
                         style.knob.paint(r, b, s);
                     }});
    cases.push_back({"dropdown", style.dropdown.margin(), [&](IRenderer& r, const Rect& b) {
                         DropdownState s;
                         s.theme = th;
                         s.label = "TYPE";
                         s.valueText = "A value text that is much too long";
                         s.open = true;
                         s.focused = true;
                         style.dropdown.paint(r, b, s);
                     }});
    cases.push_back({"stepper", style.stepper.margin(), [&](IRenderer& r, const Rect& b) {
                         StepperState s;
                         s.theme = th;
                         s.label = "Semitones";
                         s.valueText = "+12 st and then some";
                         s.hotPart = 1;
                         s.focused = true;
                         style.stepper.paint(r, b, s);
                     }});
    cases.push_back({"panel", style.panel.margin(), [&](IRenderer& r, const Rect& b) {
                         PanelState s;
                         s.theme = th;
                         s.title = "A panel title";
                         s.accent = rgb(112, 170, 255);
                         style.panel.paint(r, b, s);
                     }});
    cases.push_back({"text field", style.textField.margin(), [&](IRenderer& r, const Rect& b) {
                         TextFieldState s;
                         s.theme = th;
                         s.text = "Some typed text that is much too long for the field";
                         s.caret = true;
                         style.textField.paint(r, b, s);
                     }});

    const Rect sizes[] = {{60, 50, 120, 100}, {70, 60, 70, 40}, {60, 60, 150, 28}};
    for (const PaintCase& c : cases)
        for (const Rect& b : sizes) {
            SoftRenderer r = makeSoft(280, 240, 2);
            r.clear(rgb(77, 88, 99));
            c.paint(r, b);
            const auto px = r.frame();
            const Rect allowed = b.expand(c.margin);
            int outside = 0;
            for (int y = 0; y < r.height(); ++y)
                for (int x = 0; x < r.width(); ++x)
                    if (!allowed.contains(x, y) && at(px, r.width(), x, y) != rgb(77, 88, 99))
                        ++outside;
            INFO(c.name << " in " << b.x << "," << b.y << " " << b.w << "x" << b.h);
            CHECK(outside == 0);
        }
}

TEST_CASE("popup and tooltip: layout stays in view, items are placed, painting stays inside") {
    ModernStyle style;
    SoftRenderer r = makeSoft(400, 260, 2);
    std::vector<PopupItem> items;
    for (int i = 0; i < 18; ++i)
        items.push_back({"Item number " + std::to_string(i)});
    items.insert(items.begin() + 3, PopupItem{"", true, true});
    items.push_back({"Disabled", false, false});
    PopupState ps;
    ps.theme = &style.theme;
    ps.items = &items;
    ps.anchor = {350, 230, 40, 20};
    ps.columns = 3;
    ps.selected = 2;
    std::vector<Rect> rects;
    const Rect box = style.popup.layout(r, ps, 400, 260, rects);
    CHECK(Rect{0, 0, 400, 260}.intersect(box) == box);
    REQUIRE(rects.size() == items.size());
    for (const Rect& it : rects)
        CHECK(box.intersect(it) == it);
    CHECK(rects[3].h < rects[2].h); // the separator is a thin row
    ps.itemRects = &rects;
    ps.hot = 5;
    r.clear(rgb(77, 88, 99));
    style.popup.paint(r, box, ps);
    const auto px = r.frame();
    const Rect allowed = box.expand(style.popup.margin());
    int outside = 0;
    for (int y = 0; y < 260; ++y)
        for (int x = 0; x < 400; ++x)
            outside += !allowed.contains(x, y) && at(px, 400, x, y) != rgb(77, 88, 99);
    CHECK(outside == 0);

    { // a row with a submenu shows a marker at its right end, and nothing else changes
        std::vector<PopupItem> sub = items;
        sub[7].children = {{"Sub"}};
        PopupState plain = ps, withSub = ps;
        plain.hot = withSub.hot = -1;
        withSub.items = &sub;
        r.clear(rgb(77, 88, 99));
        style.popup.paint(r, box, plain);
        const auto a = r.frame();
        r.clear(rgb(77, 88, 99));
        style.popup.paint(r, box, withSub);
        const auto b = r.frame();
        int marker = 0, elsewhere = 0;
        for (int y = 0; y < 260; ++y)
            for (int x = 0; x < 400; ++x)
                if (at(a, 400, x, y) != at(b, 400, x, y)) {
                    if (rects[7].contains(x, y) && x >= rects[7].right() - 18)
                        ++marker;
                    else
                        ++elsewhere;
                }
        CHECK(marker > 3);
        CHECK(elsewhere == 0);
        // the row whose submenu is open is drawn like the hot one
        PopupState opened = withSub;
        opened.open = 7;
        r.clear(rgb(77, 88, 99));
        style.popup.paint(r, box, opened);
        PopupState hotOne = withSub;
        hotOne.hot = 7;
        const auto o = r.frame();
        r.clear(rgb(77, 88, 99));
        style.popup.paint(r, box, hotOne);
        CHECK(o == r.frame());
    }

    TooltipState ts;
    ts.theme = &style.theme;
    ts.title = "Cutoff";
    ts.text = "1.20 kHz   modulatable";
    ts.pointer = {390, 250};
    const Rect tip = style.tooltip.layout(r, ts, 400, 260);
    CHECK(Rect{0, 0, 400, 260}.intersect(tip) == tip);
    CHECK_FALSE(tip.contains(390, 250)); // not under the pointer

    // a long body never makes the tooltip wider than the view
    ts.text = "a very long description of a parameter that is much wider than this small view could ever hold";
    const Rect wide = style.tooltip.layout(r, ts, 200, 260);
    CHECK(Rect{0, 0, 200, 260}.intersect(wide) == wide);
}

TEST_CASE("UTF-8 decoding: one '?' per malformed sequence, four-byte forms are whole characters") {
    using spux::ui::modern::FontAtlas;
    const auto decode = [](std::string_view s) {
        std::vector<std::uint32_t> out;
        for (std::size_t i = 0; i < s.size();)
            out.push_back(FontAtlas::nextCodePoint(s, i));
        return out;
    };
    CHECK(decode("a\xC3\xA9\xE2\x80\xA6") == std::vector<std::uint32_t>{'a', 0xE9, 0x2026});
    CHECK(decode("\xF0\x9F\x98\x80") == std::vector<std::uint32_t>{0x1F600});
    CHECK(decode("\xF0\x9F\x98") == std::vector<std::uint32_t>{'?'});     // truncated
    CHECK(decode("\x80\x80x") == std::vector<std::uint32_t>{'?', 'x'});   // stray continuations
    CHECK(decode("\xC0\x80") == std::vector<std::uint32_t>{'?'});         // overlong
    CHECK(decode("\xED\xA0\x80") == std::vector<std::uint32_t>{'?'});     // surrogate
    CHECK(decode("\xF4\x90\x80\x80") == std::vector<std::uint32_t>{'?'}); // beyond U+10FFFF
}

TEST_CASE("the knob's hit area and depth handle follow its geometry") {
    ModernStyle style;
    const Rect b{40, 30, 100, 130};
    const Rect dial = style.knob.hitArea(b);
    CHECK(b.intersect(dial) == dial);
    CHECK(dial.w == dial.h);
    Point lo, hi;
    REQUIRE(style.knob.depthHandle(b, 0.0, lo));
    REQUIRE(style.knob.depthHandle(b, 1.0, hi));
    // both ends of the sweep are below the center line and on the same ring
    const Point c = dial.center();
    CHECK(lo.y > c.y);
    CHECK(hi.y > c.y);
    CHECK(std::hypot(lo.x - c.x, lo.y - c.y) == doctest::Approx(std::hypot(hi.x - c.x, hi.y - c.y)).epsilon(0.01));
    CHECK(lo.x < c.x);
    CHECK(hi.x > c.x);
}

TEST_CASE("ModernStyle through the Gui: a knob drag is one gesture, a slider jumps, a selector picks") {
    Host host;
    ModernStyle style;
    SoftRenderer r = makeSoft(kPageW, kPageH, 1);
    MGui gui(host, style);
    gui.setCaching(false);
    ShowData d;
    auto frame = [&] {
        r.clear(style.theme.colors.background);
        gui.begin(r, 1.0, 0.016f, kPageW, kPageH);
        showcaseWidgets(gui, style, d);
        gui.end();
    };
    frame();
    const Rect knob{146, 56, 80, 100};
    const Rect dial = style.knob.hitArea(knob);
    const Point c = dial.center();
    const double before = host.plainOf(P_osc_a_phase_motion_rate);
    gui.onMouseDown(c.x, c.y, MouseButton::Left, false);
    frame();
    gui.onMouseMove(c.x, c.y - 40.0f);
    frame();
    gui.onMouseUp(c.x, c.y - 40.0f, MouseButton::Left);
    frame();
    const auto kinds = host.drain();
    REQUIRE(kinds.size() >= 3);
    CHECK(kinds.front() == ParamEdit::Kind::Begin);
    CHECK(kinds.back() == ParamEdit::Kind::End);
    CHECK(host.plainOf(P_osc_a_phase_motion_rate) > before);

    // the selector's cells are the painter's: a press in the third cell sets the third value
    const Rect sel{36, 232, 100, 26};
    SelectorState three;
    three.count = 3;
    const Rect third = style.selector.cell(sel, three, 2);
    gui.onMouseDown(static_cast<float>(third.x + third.w / 2), static_cast<float>(third.y + third.h / 2),
                    MouseButton::Left, false);
    frame();
    gui.onMouseUp(static_cast<float>(third.x + third.w / 2), static_cast<float>(third.y + third.h / 2),
                  MouseButton::Left);
    frame();
    CHECK(host.plainOf(P_quality) == 2.0);
}

TEST_CASE("the config changes the look; configure() applies to the whole style") {
    Host host;
    ShowData d;
    auto render = [&](ModernStyle& s) {
        SoftRenderer r = makeSoft(kPageW, kPageH, 1);
        showcaseFrame(r, host, s, d);
        return r.frame();
    };
    ModernStyle style;
    const auto base = render(style);

    ModernConfig flat;
    flat.effects.glow = false;
    flat.shape.panelShadow = false;
    ModernStyle plain(flat);
    const auto flatFrame = render(plain);
    CHECK(diffCount(base, flatFrame) > 1000);

    ModernConfig contrast = ModernConfig::highContrast();
    CHECK(contrast.theme.colors.text == rgb(255, 255, 255));
    ModernStyle other;
    other.configure(contrast);
    CHECK(other.theme.colors.text == rgb(255, 255, 255));
    CHECK(other.config().theme.colors.text == rgb(255, 255, 255));
    const auto contrastFrame = render(other);
    CHECK(diffCount(base, contrastFrame) > 1000);
    // the same config gives the same pixels
    ModernStyle again(flat);
    CHECK(diffCount(flatFrame, render(again)) == 0);

    ModernConfig round = ModernConfig{};
    round.knob.startAngle = 2.0f;
    round.knob.sweep = 5.2f;
    round.toggle.width = 40.0f;
    ModernStyle custom(round);
    CHECK(diffCount(base, render(custom)) > 100);
}

TEST_CASE("showcase on the software renderer: deterministic, filled, a golden frame, quality levels agree") {
    Host host;
    ShowData d;
    ModernStyle style;
    SoftRenderer r = makeSoft(kPageW, kPageH, 2);
    showcaseFrame(r, host, style, d);
    const auto a = r.frame();
    SoftRenderer r2 = makeSoft(kPageW, kPageH, 2);
    showcaseFrame(r2, host, style, d);
    CHECK(diffCount(a, r2.frame()) == 0);
    long lit = 0;
    for (std::uint32_t p : a)
        lit += luma(p) > 60;
    CHECK(lit > 15000);
    snapshot("modern_soft", a, kPageW, kPageH);
    checkGolden("modern_soft", a, kPageW, kPageH, 0.98);

    SoftRenderer fast = makeSoft(kPageW, kPageH, 1);
    showcaseFrame(fast, host, style, d);
    CHECK(ssim(fast.frame(), a, kPageW, kPageH) > 0.9);
}

TEST_CASE("the editor shell: cached frames equal uncached frames, and only what changed is redrawn") {
    Host host;
    ModernStyle style;
    ShowRoot root(style);
    EditorOptions o;
    o.width = kPageW;
    o.height = kPageH;
    Editor<ModernStyle> ed(host, style, root, o,
                           [](int w, int h) { return std::make_unique<SoftRenderer>(w, h, 2, sharedAtlas()); });
    ed.gui().setOverlapCheck(true);
    REQUIRE(ed.frame(false, 0.0));
    CHECK(ed.gui().overlapCount() == 0);
    CHECK(ed.gui().idCollisions() == 0);
    const std::vector<std::uint32_t> first(ed.pixels(), ed.pixels() + kPageW * kPageH);
    CHECK_FALSE(ed.frame(false, 0.1));

    host.set(P_osc_a_level, -6.0);
    REQUIRE(ed.frame(false, 0.2));
    INFO("painted " << ed.gui().paintedCount() << " dirty " << ed.lastDirty().x << "," << ed.lastDirty().y << " "
                    << ed.lastDirty().w << "x" << ed.lastDirty().h);
    CHECK(ed.gui().paintedCount() == 1);
    CHECK(ed.lastDirty().w < kPageW / 2); // one knob and what it overlaps, not the page
    const std::vector<std::uint32_t> cached(ed.pixels(), ed.pixels() + kPageW * kPageH);

    // the same state drawn from scratch
    Host host2;
    host2.set(P_osc_a_level, -6.0);
    ModernStyle style2;
    ShowData d2;
    SoftRenderer r = makeSoft(kPageW, kPageH, 2);
    r.clear(style2.theme.colors.background);
    r.drawBackground(kBlue, kViolet, 6.0f);
    MGui gui(host2, style2);
    gui.setCaching(false);
    gui.begin(r, 0.2, 0.016f, kPageW, kPageH);
    showcasePanels(gui);
    showcaseWidgets(gui, style2, d2);
    gui.end();
    const auto fresh = r.frame();
    const int diff = diffCount(cached, fresh);
    INFO("differing pixels " << diff);
    // panels draw under the widgets in both, so the frames agree except where a widget's margin meets a
    // translucent panel edge
    CHECK(diff < kPageW * kPageH / 200);
    CHECK(ssim(cached, fresh, kPageW, kPageH) > 0.995);
}

TEST_CASE("the quality governor lowers effects when over budget and raises them with room to spare") {
    QualityGovernor g(6.0f);
    CHECK(g.level() == 0);
    for (int i = 0; i < 300; ++i)
        g.update(0.0f); // no timer queries: no change
    CHECK(g.level() == 0);
    for (int i = 0; i < 60; ++i)
        g.update(2.0f); // comfortable: stays at full quality
    CHECK(g.level() == 0);
    int steps = 0, last = 0;
    for (int i = 0; i < 4000 && g.level() < QualityGovernor::kMaxLevel; ++i) {
        g.update(12.0f); // over budget
        if (g.level() != last) {
            ++steps;
            last = g.level();
        }
    }
    CHECK(g.level() == QualityGovernor::kMaxLevel);
    CHECK(steps == QualityGovernor::kMaxLevel); // one level at a time
    for (int i = 0; i < 20000 && g.level() > 0; ++i)
        g.update(1.0f); // plenty of room
    CHECK(g.level() == 0);
    g.update(12.0f);
    g.reset();
    CHECK(g.level() == 0);
}

TEST_CASE("quality levels step the effects down in order") {
    const GpuRenderer::Quality full = qualityForLevel(0);
    CHECK(full.bloom);
    CHECK(full.bloomLevels == 5);
    CHECK(full.grain);
    CHECK(full.animatedBackground);
    int levels = 5;
    for (int l = 1; l <= QualityGovernor::kMaxLevel; ++l) {
        const GpuRenderer::Quality q = qualityForLevel(l);
        CHECK(q.bloomLevels <= levels);
        levels = q.bloomLevels;
    }
    CHECK(qualityForLevel(2).bloomDownscale);
    CHECK_FALSE(qualityForLevel(1).bloomDownscale);
    CHECK_FALSE(qualityForLevel(4).bloom);
    CHECK_FALSE(qualityForLevel(4).animatedBackground);
    CHECK_FALSE(qualityForLevel(0, true).grain); // reduced motion: no grain, a still backdrop
    CHECK_FALSE(qualityForLevel(0, true).animatedBackground);
    CHECK(qualityForLevel(99).bloomLevels == qualityForLevel(QualityGovernor::kMaxLevel).bloomLevels); // clamped
}

TEST_CASE("modernBackend: a surface to try first unless software is asked for, and the fallback renderer") {
    const ModernBackend gpu = modernBackend();
    CHECK(static_cast<bool>(gpu.gpu));
    SurfaceOptions soft;
    soft.choice = RendererChoice::Software;
    const ModernBackend none = modernBackend(soft, 1);
    if (!std::getenv("SPUX_RENDERER"))
        CHECK_FALSE(static_cast<bool>(none.gpu)); // the environment may override the choice
    auto r = none.software(64, 48);
    REQUIRE(r != nullptr);
    CHECK(r->width() == 64);
    CHECK(r->height() == 48);
    CHECK(r->layers() != nullptr);
    CHECK_FALSE(r->isGpu());
    auto surface = gpu.gpu();
    REQUIRE(surface != nullptr);
    CHECK(surface->renderer() == nullptr); // nothing exists before attach
    CHECK(surface->degradation() == 0);
    surface->detach(); // safe without an attach, and twice
    surface->detach();
}
