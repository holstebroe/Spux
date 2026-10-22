// spux_ui_classic: the shaded renderer, ClassicStyle's painters (inside their bounds, with the geometry the
// interaction asks for), the two looks as golden frames, the Gui and the editor shell on top of them.
#include <doctest.h>

#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "ui/ClassicSupport.hpp"
#include "ui/TestRenderer.hpp"

using namespace classic_test;
namespace c = spux::ui::classic;

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

// The three shipped looks.
ClassicConfig lookConfig(int look) {
    return look == 0 ? ClassicConfig::slate() : look == 1 ? ClassicConfig::silver() : ClassicConfig::forge();
}
std::string lookName(int look) { return look == 0 ? "classic_slate" : look == 1 ? "classic_silver" : "classic_forge"; }

int lumaAt(const std::vector<std::uint32_t>& px, int w, int x, int y) { return ui_test::luma(at(px, w, x, y)); }

} // namespace

// --- the renderer ------------------------------------------------------------------------------------------

TEST_CASE("shaded renderer: fills, clip, alpha") {
    ShadedRenderer r(64, 48, 2);
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
}

TEST_CASE("shaded renderer: shapes have soft edges and the right area at every supersampling") {
    for (int ss = 1; ss <= 4; ++ss) {
        ShadedRenderer r(40, 40, ss);
        CHECK(r.supersample() == ss);
        r.clear(rgb(0, 0, 0));
        r.fillCircle({20.0f, 20.0f}, 10.0f, rgb(255, 255, 255));
        double sum = 0;
        bool partial = false;
        for (std::uint32_t p : r.frame()) {
            const double v = static_cast<double>(p & 255u) / 255.0;
            sum += v;
            partial = partial || (v > 0.05 && v < 0.95);
        }
        INFO("supersample " << ss);
        CHECK(partial);
        CHECK(sum == doctest::Approx(3.14159 * 100.0).epsilon(0.03));
    }
}

TEST_CASE("shaded renderer: rounded boxes, rings, gradients, lines, glow and shadows stay inside the clip") {
    ShadedRenderer r(120, 80, 2);
    r.clear(rgb(10, 10, 10));
    const Rect clip{20, 10, 60, 50};
    r.pushClip(clip);
    r.fillRoundRect({0, 0, 120, 80}, 12.0f, rgb(200, 100, 50));
    r.strokeRoundRect({-5, -5, 130, 90}, 12.0f, rgb(50, 200, 100), 3.0f);
    r.strokeCircle({60.0f, 40.0f}, 30.0f, rgb(255, 255, 255), 2.0f);
    r.fillGradientRect({0, 0, 120, 80}, rgb(0, 0, 0), rgb(255, 255, 255));
    r.drawLine({-10.0f, -10.0f}, {200.0f, 100.0f}, rgb(255, 0, 0), 4.0f);
    const Point poly[3] = {{0.0f, 80.0f}, {60.0f, 0.0f}, {120.0f, 80.0f}};
    r.drawPolyline(poly, 3, rgb(0, 0, 255), 3.0f);
    r.drawGlow({60.0f, 40.0f}, 60.0f, rgb(255, 255, 0));
    r.fillRadialGradient({60.0f, 40.0f}, 50.0f, rgb(255, 255, 255), rgb(0, 0, 0));
    r.drawShadow({30, 20, 40, 30}, 6.0f, 20.0f, rgb(0, 0, 0, 200));
    r.drawText("CLIPPED TEXT RUNS PAST THE EDGE", 0, 30, rgb(255, 255, 255), 2);
    r.drawTextPx("Pixel sized", 0.0f, 12.0f, rgb(255, 255, 255), 14.0f, Align::Left, FontWeight::Bold);
    r.ink(r.labelFont(), "INK", 70.0f, 45.0f, 8.0f, rgb(255, 255, 255), 0.2f);
    r.popClip();
    const auto px = r.frame();
    int outside = 0, inside = 0;
    for (int y = 0; y < 80; ++y)
        for (int x = 0; x < 120; ++x) {
            const bool changed = at(px, 120, x, y) != rgb(10, 10, 10);
            if (!clip.contains(x, y))
                outside += changed;
            else
                inside += changed;
        }
    CHECK(outside == 0);
    CHECK(inside > 2000);
}

TEST_CASE("shaded renderer: text uses the atlas fonts and matches its metrics") {
    ShadedRenderer r(240, 60, 2);
    r.clear(rgb(0, 0, 0));
    r.drawTextPx("Cutoff 1.2 kHz", 10.0f, 10.0f, rgb(255, 255, 255), 16.0f);
    r.drawTextPx("Right", 230.0f, 34.0f, rgb(255, 255, 255), 12.0f, Align::Right);
    const auto px = r.frame();
    int top = 0, bottom = 0;
    for (int y = 0; y < 60; ++y)
        for (int x = 0; x < 240; ++x) {
            const bool on = lumaAt(px, 240, x, y) > 100;
            if (y < 30)
                top += on;
            else {
                bottom += on;
                if (on)
                    CHECK(x < 231);
            }
        }
    CHECK(top > 100);
    CHECK(bottom > 30);
    CHECK(r.textWidthPx("Cutoff 1.2 kHz", 16.0f) ==
          doctest::Approx(c::textWidth(r.labelFont(), "Cutoff 1.2 kHz", 16.0f * 0.7f)));
    CHECK(r.textWidth("WWW", 2) == 2 * r.textWidth("WWW", 1));
    CHECK(r.textHeight(3) == 3 * r.textHeight(1));
    CHECK(r.textWidthPx("MM", 12.0f, FontWeight::Bold) != r.textWidthPx("MM", 12.0f));
}

TEST_CASE("shaded renderer: the layers behave as the editor expects") {
    ShadedRenderer r(50, 50, 2);
    REQUIRE(r.layers() != nullptr);
    CHECK_FALSE(r.isGpu());
    r.cpu().clearDirty();
    r.cpu().markDirty({10, 10, 5, 5});
    CHECK(r.cpu().dirtyBounds() == Rect{10, 10, 5, 5});
}

TEST_CASE("Graphics clip: blending and shading stop at the clip, the default is the whole buffer") {
    std::vector<std::uint32_t> buf(40 * 2 * 40 * 2, 0);
    c::Graphics g(buf.data(), 40, 40, 2);
    CHECK(g.clipX1() == 80);
    g.setClip(20, 20, 40, 40);
    c::shadeBox(g, 0, 0, 40, 40, [](float, float) { return 0xFFFFFFFFu; });
    long lit = 0;
    for (int y = 0; y < 80; ++y)
        for (int x = 0; x < 80; ++x) {
            const bool on = buf[static_cast<std::size_t>(y) * 80 + static_cast<std::size_t>(x)] != 0;
            lit += on;
            if (on)
                CHECK((x >= 20 && x < 40 && y >= 20 && y < 40));
        }
    CHECK(lit == 400);
    g.setClip(-10, -10, 1000, 1000); // clamped to the buffer
    CHECK(g.clipX0() == 0);
    CHECK(g.clipX1() == 80);
}

TEST_CASE("the worn panel: rendered once per size, identical to paintWornPanel, clipped") {
    ShadedRenderer r(120, 80, 2);
    r.wornPanel({0, 0, 120, 80}, true);
    const auto first = r.frame();
    // the same pixels as the drawing code makes for a buffer of that size
    std::vector<std::uint32_t> buf(120 * 2 * 80 * 2, 0);
    c::Graphics g(buf.data(), 120, 80, 2);
    c::paintWornPanel(g, 120, 80, true);
    CpuLayers ref(120, 80, 2);
    std::copy(buf.begin(), buf.end(), ref.target());
    std::vector<std::uint32_t> expect(120 * 80);
    ref.resolve(expect.data(), {0, 0, 120, 80});
    CHECK(diffCount(first, expect) == 0);
    // a second draw copies the cache; clear and redraw gives the same frame
    r.clear(rgb(0, 0, 0));
    r.wornPanel({0, 0, 120, 80}, true);
    CHECK(diffCount(first, r.frame()) == 0);
    // trims off is another panel; a clip limits the copy
    r.clear(rgb(1, 2, 3));
    r.pushClip({10, 10, 30, 30});
    r.wornPanel({0, 0, 120, 80}, false);
    r.popClip();
    const auto part = r.frame();
    CHECK(at(part, 120, 5, 5) == rgb(1, 2, 3));
    CHECK(at(part, 120, 20, 20) != rgb(1, 2, 3));
    // a panel drawn at an offset takes the texture of its own size
    r.clear(rgb(1, 2, 3));
    r.wornPanel({30, 20, 60, 40}, true);
    CHECK(at(r.frame(), 120, 10, 10) == rgb(1, 2, 3));
    CHECK(at(r.frame(), 120, 40, 30) != rgb(1, 2, 3));
}

// --- the painters --------------------------------------------------------------------------------------------

namespace {
struct PaintCase {
    std::string name;
    Margin margin;
    std::function<void(IRenderer&, const Rect&)> paint;
};
} // namespace

TEST_CASE("every painter draws inside its bounds plus its margin, in both looks") {
    for (int look = 0; look < 3; ++look) {
        ClassicStyle style(lookConfig(look));
        const Theme* th = &style.theme;
        const Argb own = rgb(232, 150, 40);
        const params::ParamInfo* info = &kInfos[0]; // quality: Enum
        const char* labels[3] = {"Aaaaaaaaaaaaaaaa", "Bbbbbbbbbbbbbbbbbbb", "C"};

        std::vector<PaintCase> cases;
        for (int v = 0; v < 6; ++v)
            cases.push_back(
                {"knob v=" + std::to_string(v), style.knob.margin(), [=, &style](IRenderer& r, const Rect& b) {
                     KnobState s;
                     s.theme = th;
                     s.label = "A VERY LONG LABEL THAT CANNOT FIT";
                     s.valueText = "12345.678 kHz and more";
                     s.accent = v % 2 ? own : th->colors.accent;
                     s.value = v == 1 ? 0.0 : v == 2 ? 1.0 : 0.63;
                     s.hot = v == 3;
                     s.focused = v == 3 || v == 4;
                     s.bipolar = v == 4;
                     s.dimmed = v == 5;
                     s.editing = v == 5;
                     s.editText = "-12.5 dB entered long text";
                     s.caret = true;
                     s.pickable = v == 2 || v == 4;
                     s.mod.any = v >= 3;
                     s.mod.base = 0.4;
                     s.mod.shared = v == 4 ? 0.1 : 0.9;
                     style.knob.paint(r, b, s);
                 }});
        for (int vertical = 0; vertical < 2; ++vertical)
            for (int v = 0; v < 3; ++v)
                cases.push_back({vertical ? "slider vertical" : "slider horizontal", style.slider.margin(),
                                 [=, &style](IRenderer& r, const Rect& b) {
                                     SliderState s;
                                     s.theme = th;
                                     s.vertical = vertical != 0;
                                     s.value = v == 0 ? 0.0 : v == 1 ? 0.5 : 1.0;
                                     s.param = v == 1 ? &kInfos[4] : nullptr; // osc_a_level: bipolar
                                     s.label = v == 2 ? "" : "A long label for the slider";
                                     s.valueText = v == 1 ? "" : "+12.34 dB value";
                                     s.accent = v == 2 ? own : th->colors.accent;
                                     s.hot = v == 1;
                                     s.focused = v == 1;
                                     style.slider.paint(r, b, s);
                                 }});
        for (int on = 0; on < 2; ++on)
            cases.push_back({"toggle", style.toggle.margin(), [=, &style](IRenderer& r, const Rect& b) {
                                 ToggleState s;
                                 s.theme = th;
                                 s.on = on != 0;
                                 s.label = "A toggle label that is too long to fit";
                                 s.accent = on ? own : 0;
                                 s.hot = on != 0;
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
                                 s.accent = own;
                                 s.focused = true;
                                 style.selector.paint(r, b, s);
                                 s.labels = nullptr;
                                 s.param = info;
                                 style.selector.paint(r, b, s);
                                 s.param = nullptr;
                                 style.selector.paint(r, b, s);
                             }});
        for (int variant = 0; variant < 5; ++variant)
            for (int on = 0; on < 2; ++on)
                cases.push_back({"button v=" + std::to_string(variant), style.button.margin(),
                                 [=, &style](IRenderer& r, const Rect& b) {
                                     ButtonState s;
                                     s.theme = th;
                                     s.variant = variant;
                                     s.on = on != 0;
                                     s.hot = on == 0;
                                     s.active = on != 0;
                                     s.focused = true;
                                     s.label =
                                         variant == 1 ? "A LONG LEGEND\nOVER TWO LINES" : "A BUTTON WITH A LONG LABEL";
                                     s.accent = own;
                                     style.button.paint(r, b, s);
                                 }});
        for (int titled = 0; titled < 2; ++titled)
            cases.push_back({titled ? "panel frame" : "panel surface", style.panel.margin(),
                             [=, &style](IRenderer& r, const Rect& b) {
                                 PanelState s;
                                 s.theme = th;
                                 s.title = titled ? "A panel title that is far too long for the frame" : "";
                                 style.panel.paint(r, b, s);
                             }});

        for (int on = 0; on < 2; ++on)
            cases.push_back({"led", style.led.margin(), [=, &style](IRenderer& r, const Rect& b) {
                                 LedState s;
                                 s.theme = th;
                                 s.on = on != 0;
                                 s.level = on ? 0.6f : 1.0f;
                                 s.label = "A LAMP WITH A LONG LABEL";
                                 s.accent = own;
                                 style.led.paint(r, b, s);
                             }});
        for (int v = 0; v < 3; ++v)
            cases.push_back({"keyboard", style.keyboard.margin(), [=, &style](IRenderer& r, const Rect& b) {
                                 std::uint8_t down[128] = {}, sounding[128] = {};
                                 down[v == 1 ? 49 : 48] = 1;
                                 sounding[52] = sounding[v == 2 ? 70 : 66] = 1;
                                 KeyboardState s;
                                 s.theme = th;
                                 s.first = v == 2 ? 47 : 48; // a keyboard may begin on a black key
                                 s.last = v == 2 ? 70 : 71;
                                 s.down = down;
                                 s.lit = sounding;
                                 s.hotKey = v == 0 ? 50 : -1;
                                 s.accent = own;
                                 style.keyboard.paint(r, b, s);
                             }});
        for (int open = 0; open < 2; ++open) {
            cases.push_back({"dropdown", style.dropdown.margin(), [=, &style](IRenderer& r, const Rect& b) {
                                 DropdownState s;
                                 s.theme = th;
                                 s.label = "TYPE";
                                 s.valueText = "A value text that is much too long";
                                 s.open = open != 0;
                                 s.hot = open == 0;
                                 s.focused = true;
                                 style.dropdown.paint(r, b, s);
                             }});
            cases.push_back({"stepper", style.stepper.margin(), [=, &style](IRenderer& r, const Rect& b) {
                                 StepperState s;
                                 s.theme = th;
                                 s.label = "Semitones";
                                 s.valueText = "+12 st and then some";
                                 s.hotPart = open ? 1 : -1;
                                 s.focused = true;
                                 style.stepper.paint(r, b, s);
                             }});
        }
        cases.push_back({"text field", style.textField.margin(), [=, &style](IRenderer& r, const Rect& b) {
                             TextFieldState s;
                             s.theme = th;
                             s.text = "Some typed text that is much too long for the field";
                             s.caret = true;
                             style.textField.paint(r, b, s);
                         }});
        cases.push_back({"icons", style.button.margin(), [=, &style](IRenderer& r, const Rect& b) {
                             ButtonState s;
                             s.theme = th;
                             s.icon = waveIcon(Wave::Saw);
                             s.label = "SAW WITH A LONG LABEL";
                             style.button.paint(r, b, s);
                             s.label = "";
                             s.variant = kButtonCap;
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
                             static const char* const labels[3] = {"LFO 1", "A VERY LONG TAB NAME", "MOD"};
                             TabsState s;
                             s.theme = th;
                             s.count = 3;
                             s.index = 1;
                             s.hotIndex = 2;
                             s.labels = labels;
                             s.focused = true;
                             s.accent = own;
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
                     style.button.paint(r, b, s);
                 }});

        const Rect sizes[] = {{60, 50, 120, 100}, {70, 60, 70, 40}, {60, 60, 150, 28}, {80, 70, 3, 3}};
        for (const PaintCase& pc : cases)
            for (const Rect& b : sizes) {
                ShadedRenderer r(280, 240, 2);
                r.clear(rgb(77, 88, 99));
                pc.paint(r, b);
                const auto px = r.frame();
                const Rect allowed = b.expand(pc.margin);
                int outside = 0, inside = 0;
                for (int y = 0; y < r.height(); ++y)
                    for (int x = 0; x < r.width(); ++x) {
                        const bool changed = at(px, r.width(), x, y) != rgb(77, 88, 99);
                        if (!allowed.contains(x, y))
                            outside += changed;
                        else
                            inside += changed;
                    }
                INFO((look ? "silver " : "slate ")
                     << pc.name << " in " << b.x << "," << b.y << " " << b.w << "x" << b.h);
                CHECK(outside == 0);
                if (b.w >= 70 && b.h >= 40)
                    CHECK(inside > 0);
            }
    }
}

TEST_CASE("the fader's groove width and end, and its scale on both sides, are options; the defaults keep the look") {
    auto render = [](const ClassicConfig& c) {
        ClassicStyle style(c);
        ShadedRenderer r(60, 120, 2);
        r.clear(rgb(40, 44, 50));
        SliderState s;
        s.theme = &style.theme;
        s.vertical = true;
        s.value = 0.3;
        style.slider.paint(r, {5, 5, 50, 110}, s);
        return r.frame();
    };
    ClassicConfig def = ClassicConfig::slate();
    ClassicConfig narrow = def;
    narrow.slider.slotW = 4.0f;
    narrow.slider.slotEnd = 2.0f;
    ClassicConfig both = def;
    both.slider.scaleBothSides = true;
    const auto d = render(def);
    CHECK(diffCount(d, render(ClassicConfig::slate())) == 0);
    CHECK(diffCount(d, render(narrow)) > 0);
    const auto b = render(both);
    // the right half gains the ticks, the left half is as before
    int left = 0, right = 0;
    for (int y = 0; y < 120; ++y)
        for (int x = 0; x < 60; ++x)
            if (at(d, 60, x, y) != at(b, 60, x, y))
                (x < 30 ? left : right)++;
    CHECK(left == 0);
    CHECK(right > 20);
}

TEST_CASE("a painter on another renderer draws nothing") {
    ClassicStyle style;
    uitest::TestRenderer r(100, 100);
    KnobState k;
    k.theme = &style.theme;
    style.knob.paint(r, {0, 0, 80, 80}, k);
    SliderState sl;
    sl.theme = &style.theme;
    style.slider.paint(r, {0, 0, 80, 80}, sl);
    ToggleState t;
    t.theme = &style.theme;
    style.toggle.paint(r, {0, 0, 80, 80}, t);
    SelectorState se;
    se.theme = &style.theme;
    se.count = 2;
    style.selector.paint(r, {0, 0, 80, 80}, se);
    ButtonState b;
    b.theme = &style.theme;
    style.button.paint(r, {0, 0, 80, 80}, b);
    PanelState p;
    p.theme = &style.theme;
    style.panel.paint(r, {0, 0, 80, 80}, p);
    LedState l;
    l.theme = &style.theme;
    style.led.paint(r, {0, 0, 80, 20}, l);
    KeyboardState kb;
    kb.theme = &style.theme;
    style.keyboard.paint(r, {0, 0, 80, 40}, kb);
    DropdownState dd;
    dd.theme = &style.theme;
    style.dropdown.paint(r, {0, 0, 80, 20}, dd);
    StepperState st;
    st.theme = &style.theme;
    style.stepper.paint(r, {0, 0, 80, 20}, st);
    TextFieldState tf;
    tf.theme = &style.theme;
    style.textField.paint(r, {0, 0, 80, 20}, tf);
    TabsState tb;
    tb.theme = &style.theme;
    tb.count = 2;
    style.tabs.paint(r, {0, 0, 80, 20}, tb);
    ReadoutState ro;
    ro.theme = &style.theme;
    style.readout.paint(r, {0, 0, 80, 20}, ro);
    CHECK(r.calls == 0);
}

TEST_CASE("the lamp: lit is brighter than unlit, glows beyond its lens, and follows its level") {
    auto render = [](bool lit, float level = 1.0f) {
        ShadedRenderer r(60, 60, 2);
        r.clear(rgb(40, 40, 44));
        drawLed(r, {30.0f, 30.0f}, 4.0f, rgb(255, 40, 30), lit, level);
        return r.frame();
    };
    const auto off = render(false), on = render(true);
    const auto dim = render(true, 0.4f);
    CHECK(((at(dim, 60, 30, 30) >> 16) & 255u) < ((at(on, 60, 30, 30) >> 16) & 255u));
    CHECK(((at(dim, 60, 30, 30) >> 16) & 255u) > ((at(off, 60, 30, 30) >> 16) & 255u));
    CHECK(((at(on, 60, 30, 30) >> 16) & 255u) > ((at(off, 60, 30, 30) >> 16) & 255u) + 80);
    CHECK(((at(on, 60, 30, 37) >> 16) & 255u) > ((at(off, 60, 30, 37) >> 16) & 255u)); // the halo
    CHECK(at(on, 60, 30, 48) == rgb(40, 40, 44));                                      // and it ends
    CHECK(diffCount(on, render(true)) == 0);
}

TEST_CASE("popup and tooltip: layout stays in view, items are placed, painting stays inside, in both looks") {
    for (int look = 0; look < 3; ++look) {
        ClassicStyle style(lookConfig(look));
        ShadedRenderer r(400, 260, 2);
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
        int outside = 0, inside = 0;
        for (int y = 0; y < 260; ++y)
            for (int x = 0; x < 400; ++x) {
                const bool changed = at(px, 400, x, y) != rgb(77, 88, 99);
                outside += !allowed.contains(x, y) && changed;
                inside += allowed.contains(x, y) && changed;
            }
        CHECK(outside == 0);
        CHECK(inside > 5000);

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
                        if (rects[7].contains(x, y) && x >= rects[7].right() - 14)
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
        r.clear(rgb(77, 88, 99));
        style.tooltip.paint(r, tip, ts);
        const auto tp = r.frame();
        const Rect tipAllowed = tip.expand(style.tooltip.margin());
        int tipOutside = 0;
        for (int y = 0; y < 260; ++y)
            for (int x = 0; x < 400; ++x)
                tipOutside += !tipAllowed.contains(x, y) && at(tp, 400, x, y) != rgb(77, 88, 99);
        CHECK(tipOutside == 0);
        // a long body never makes the tooltip wider than the view
        ts.text = "a very long description of a parameter that is much wider than this small view could ever hold";
        const Rect wide = style.tooltip.layout(r, ts, 200, 260);
        CHECK(Rect{0, 0, 200, 260}.intersect(wide) == wide);
    }
}

// --- icons ---------------------------------------------------------------------------------------------------

namespace {
int customCalls = 0;
Rect customArea;
const void* customData = nullptr;
void customIcon(IRenderer& r, const Rect& area, Argb c, float width, const void* data) {
    ++customCalls;
    customArea = area;
    customData = data;
    r.fillRect(area.inset(2), withAlpha(c, width > 0 ? 1.0f : 0.0f));
}
} // namespace

TEST_CASE("icons: every wave draws inside its area and differs from the others; a plugin's own icon is called") {
    std::vector<std::vector<std::uint32_t>> frames;
    for (int w = 0; w < kWaveCount; ++w) {
        ShadedRenderer r(60, 40, 2);
        r.clear(rgb(0, 0, 0));
        const Rect area{10, 10, 40, 20};
        drawWave(r, static_cast<Wave>(w), area, rgb(255, 255, 255), 1.5f);
        const auto px = r.frame();
        int on = 0, outside = 0;
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 60; ++x)
                if (at(px, 60, x, y) != rgb(0, 0, 0)) {
                    ++on;
                    outside += !area.contains(x, y);
                }
        INFO(waveName(static_cast<Wave>(w)));
        CHECK(on > 30);
        CHECK(outside == 0);
        frames.push_back(px);
    }
    for (int a = 0; a < kWaveCount; ++a)
        for (int b = a + 1; b < kWaveCount; ++b)
            CHECK(diffCount(frames[static_cast<std::size_t>(a)], frames[static_cast<std::size_t>(b)]) > 20);
    // the built-in icon is the wave, on any renderer
    ShadedRenderer r(60, 40, 2), q(60, 40, 2);
    r.clear(rgb(0, 0, 0));
    q.clear(rgb(0, 0, 0));
    waveIcon(Wave::Triangle)(r, {10, 10, 40, 20}, rgb(255, 255, 255), 1.5f);
    drawWave(q, Wave::Triangle, {10, 10, 40, 20}, rgb(255, 255, 255), 1.5f);
    CHECK(diffCount(r.frame(), q.frame()) == 0);
    CHECK(waveIcon(Wave::Saw) == waveIcon(Wave::Saw));
    CHECK_FALSE(waveIcon(Wave::Saw) == waveIcon(Wave::Sine));
    CHECK_FALSE(Icon{});
    uitest::TestRenderer plain(60, 40);
    waveIcon(Wave::Sine)(plain, {0, 0, 60, 40}, rgb(255, 255, 255)); // draws through the basics of any renderer
    CHECK(plain.calls > 0);

    // a plugin's own icon goes where the built-in ones go
    int token = 0;
    Icon mine{customIcon, &token};
    ClassicStyle style;
    ShadedRenderer rr(100, 50, 2);
    ButtonState s;
    s.theme = &style.theme;
    s.icon = mine;
    style.button.paint(rr, {10, 10, 80, 30}, s);
    CHECK(customCalls == 1);
    CHECK(customData == &token);
    CHECK(Rect{10, 10, 80, 30}.intersect(customArea) == customArea);
    const Icon steps[2] = {mine, waveIcon(Wave::Sine)};
    SelectorState se;
    se.theme = &style.theme;
    se.count = 2;
    se.icons = steps;
    style.selector.paint(rr, {10, 10, 80, 30}, se);
    CHECK(customCalls == 2);
}

// --- geometry the interaction asks for ----------------------------------------------------------------------

TEST_CASE(
    "the knob's hit area is the dial and follows the rows; the slider's track and selector cells follow the painters") {
    ClassicStyle slate;
    const Rect b{40, 30, 64, 80};
    const Rect dial = slate.knob.hitArea(b);
    CHECK(b.intersect(dial) == dial);
    CHECK(dial.w == dial.h);
    CHECK(dial.center().x == doctest::Approx(72.0f).epsilon(0.1));
    ClassicConfig rows = ClassicConfig::slate();
    rows.knob.topRow = rows.knob.bottomRow = 0.0f;
    ClassicStyle bare(rows);
    CHECK(bare.knob.hitArea(b).h > dial.h); // no rows, a larger dial
    CHECK(bare.knob.hitArea(b).center().y == doctest::Approx(70.0f).epsilon(0.05));

    SliderState v;
    v.vertical = true;
    const Rect t = slate.slider.track({10, 10, 44, 168}, v);
    CHECK(t.x > 10);
    CHECK(t.right() < 54);
    CHECK(t.y >= 10);
    CHECK(t.bottom() <= 178);
    SliderState h;
    const Rect th = slate.slider.track({10, 10, 168, 26}, h);
    CHECK(th.w > th.h);

    for (int vertical = 0; vertical < 2; ++vertical) {
        SelectorState s;
        s.count = 3;
        s.vertical = vertical != 0;
        const Rect sel{10, 10, 80, 90};
        int total = 0;
        for (int i = 0; i < 3; ++i) {
            const Rect cell = slate.selector.cell(sel, s, i);
            CHECK(sel.intersect(cell) == cell);
            CHECK_FALSE(cell.empty());
            total += vertical ? cell.h : cell.w;
        }
        CHECK(total <= (vertical ? 90 : 80));
        CHECK(total >= (vertical ? 80 : 78));
    }
}

TEST_CASE(
    "ClassicStyle through the Gui: a knob drag is one gesture, a selector picks, a toggle flips, a button presses") {
    Host host;
    ClassicStyle style;
    ShadedRenderer r(kPageW, kPageH, 1);
    CGui gui(host, style);
    gui.setCaching(false);
    auto frame = [&] {
        r.clear(style.theme.colors.background);
        gui.begin(r, 1.0, 0.016f, kPageW, kPageH);
        showcaseWidgets(gui);
        gui.end();
    };
    frame();
    const Rect knob{22, 40, 64, 80};
    const Point c0 = style.knob.hitArea(knob).center();
    const double before = host.plainOf(P_osc_a_phase_motion_rate);
    gui.onMouseDown(c0.x, c0.y, MouseButton::Left, false);
    frame();
    gui.onMouseMove(c0.x, c0.y - 40.0f);
    frame();
    gui.onMouseUp(c0.x, c0.y - 40.0f, MouseButton::Left);
    frame();
    const auto kinds = host.drain();
    REQUIRE(kinds.size() >= 3);
    CHECK(kinds.front() == ParamEdit::Kind::Begin);
    CHECK(kinds.back() == ParamEdit::Kind::End);
    CHECK(host.plainOf(P_osc_a_phase_motion_rate) > before);

    // the horizontal selector's cells are the painter's: a press in the third cell sets the third value
    const Rect sel{270, 236, 200, 28};
    SelectorState three;
    three.count = 3;
    const Rect third = style.selector.cell(sel, three, 2);
    const float px = static_cast<float>(third.x + third.w / 2), py = static_cast<float>(third.y + third.h / 2);
    gui.onMouseDown(px, py, MouseButton::Left, false);
    frame();
    gui.onMouseUp(px, py, MouseButton::Left);
    frame();
    CHECK(host.plainOf(P_quality) == 2.0);

    // the vertical lever's cells: the first step
    SelectorState lever;
    lever.count = 3;
    lever.vertical = true;
    const Rect first = style.selector.cell({170, 230, 80, 74}, lever, 0);
    const float lx = static_cast<float>(first.x + first.w / 2), ly = static_cast<float>(first.y + first.h / 2);
    gui.onMouseDown(lx, ly, MouseButton::Left, false);
    frame();
    gui.onMouseUp(lx, ly, MouseButton::Left);
    frame();
    CHECK(host.plainOf(P_quality) == 0.0);

    // a press on a toggle flips it
    const double mono = host.plainOf(P_mono);
    gui.onMouseDown(50.0f, 266.0f, MouseButton::Left, false);
    frame();
    gui.onMouseUp(50.0f, 266.0f, MouseButton::Left);
    frame();
    CHECK(host.plainOf(P_mono) != mono);
}

// --- the keyboard --------------------------------------------------------------------------------------------

TEST_CASE("keyboard geometry: white keys tile the bounds, black keys sit between and above, hit tests agree") {
    const Rect b{10, 20, 140, 60}; // C3 to B3: seven white keys, 20 px each
    CHECK(keyboard::whiteKeys(48, 59) == 7);
    CHECK(keyboard::isBlack(49));
    CHECK_FALSE(keyboard::isBlack(48));
    CHECK(keyboard::isBlack(-11)); // C# an octave below zero
    const auto c = keyboard::keyRect(b, 48, 59, 48);
    const auto d = keyboard::keyRect(b, 48, 59, 50);
    CHECK(c.x0 == doctest::Approx(10.0f));
    CHECK(c.x1 == doctest::Approx(30.0f));
    CHECK(d.x0 == doctest::Approx(30.0f));
    const auto cs = keyboard::keyRect(b, 48, 59, 49);
    CHECK(cs.black);
    CHECK(0.5f * (cs.x0 + cs.x1) == doctest::Approx(30.0f)); // between C and D
    CHECK(cs.y1 < static_cast<float>(b.bottom()));
    CHECK(keyboard::keyAt(b, 48, 59, {30.0f, 30.0f}) == 49); // the black key wins where it covers
    CHECK(keyboard::keyAt(b, 48, 59, {30.0f, 70.0f}) == 50); // below it, the white key
    CHECK(keyboard::keyAt(b, 48, 59, {12.0f, 70.0f}) == 48);
    CHECK(keyboard::keyAt(b, 48, 59, {149.0f, 70.0f}) == 59);
    CHECK(keyboard::keyAt(b, 48, 59, {5.0f, 70.0f}) == -1);
    // every white key hit in its middle lower half is that key
    for (int k = 48; k <= 59; ++k) {
        if (keyboard::isBlack(k))
            continue;
        const auto r = keyboard::keyRect(b, 48, 59, k);
        CHECK(keyboard::keyAt(b, 48, 59, {0.5f * (r.x0 + r.x1), 75.0f}) == k);
    }
}

TEST_CASE("keyboard through the Gui: a press plays a key with a velocity, a glide moves on, a release lets go") {
    Host host;
    ClassicStyle style;
    ShadedRenderer r(400, 100, 1);
    CGui gui(host, style);
    gui.setCaching(false);
    const Rect b{10, 10, 280, 80}; // C3 to B4
    KeyboardEvents last;
    auto frame = [&](const KeyboardOptions& o = {.first = 48, .last = 71}) {
        r.clear(style.theme.colors.background);
        gui.begin(r, 1.0, 0.016f, 400, 100);
        last = gui.keyboard("kb", b, o);
        gui.end();
        return last;
    };
    frame();
    CHECK(last.pressed == -1);
    gui.onMouseDown(20.0f, 80.0f, MouseButton::Left, false); // C3, near the foot of the key
    auto ev = frame();
    CHECK(ev.pressed == 48);
    CHECK(ev.velocity > 0.9f);
    CHECK(ev.held == 48);
    CHECK(frame().pressed == -1);  // nothing new while it is held
    gui.onMouseMove(40.0f, 80.0f); // glide to the next white key
    ev = frame();
    CHECK(ev.released == 48);
    CHECK(ev.pressed == 50);
    CHECK(ev.held == 50);
    gui.onMouseUp(40.0f, 80.0f, MouseButton::Left);
    ev = frame();
    CHECK(ev.released == 50);
    CHECK(ev.held == -1);
    // without glide the first key stays held
    gui.onMouseDown(20.0f, 14.0f, MouseButton::Left, false);
    KeyboardOptions still{.first = 48, .last = 71, .glide = false};
    ev = frame(still);
    CHECK(ev.pressed == 48);
    CHECK(ev.velocity < 0.4f); // the top of the key
    gui.onMouseMove(80.0f, 80.0f);
    ev = frame(still);
    CHECK(ev.pressed == -1);
    CHECK(ev.held == 48);
    gui.onMouseUp(80.0f, 80.0f, MouseButton::Left);
    CHECK(frame(still).released == 48);
    // a click that is over before the next frame still plays and releases the key
    gui.onMouseDown(20.0f, 80.0f, MouseButton::Left, false);
    gui.onMouseUp(20.0f, 80.0f, MouseButton::Left);
    ev = frame();
    CHECK(ev.pressed == 48);
    CHECK(ev.released == 48);
    // a press outside every key does nothing
    gui.onMouseDown(5.0f, 5.0f, MouseButton::Left, false);
    CHECK(frame().pressed == -1);
    gui.onMouseUp(5.0f, 5.0f, MouseButton::Left);
    frame();

    // sounding and held keys change what is drawn
    std::uint8_t sounding[128] = {};
    sounding[52] = 1;
    ShadedRenderer a(400, 100, 2), c(400, 100, 2);
    for (ShadedRenderer* rr : {&a, &c}) {
        rr->clear(rgb(0, 0, 0));
        CGui g2(host, style);
        g2.setCaching(false);
        g2.begin(*rr, 1.0, 0.016f, 400, 100);
        g2.keyboard("kb", b, {.first = 48, .last = 71, .lit = rr == &c ? sounding : nullptr});
        g2.end();
    }
    CHECK(diffCount(a.frame(), c.frame()) > 100);
}

// --- tabs, readout, dropdown drag, preset bar, pick -----------------------------------------------------------

TEST_CASE("tabs: a press selects and the page keeps the state; the readout shows what the pointer is on") {
    Host host;
    ClassicStyle style;
    ShadedRenderer r(400, 120, 1);
    CGui gui(host, style);
    gui.setCaching(false);
    static const char* const labels[3] = {"ONE", "TWO", "THREE"};
    int tab = 0;
    const Rect bar{10, 10, 300, 20}, knob{10, 40, 64, 80};
    auto frame = [&] {
        r.clear(style.theme.colors.background);
        gui.begin(r, 1.0, 0.016f, 400, 120);
        tab = gui.tabs("tabs", bar, labels, 3, tab);
        gui.knob(P_out_gain, knob, {.label = "GAIN"});
        gui.readout("readout", {100, 90, 200, 20}, {.idle = "IDLE"});
        gui.end();
    };
    frame();
    CHECK(tab == 0);
    TabsState probe;
    probe.count = 3;
    const Rect third = style.tabs.cell(bar, probe, 2);
    gui.onMouseDown(static_cast<float>(third.x + 5), static_cast<float>(third.y + 5), MouseButton::Left, false);
    frame();
    gui.onMouseUp(static_cast<float>(third.x + 5), static_cast<float>(third.y + 5), MouseButton::Left);
    frame();
    CHECK(tab == 2);
    gui.onMouseDown(static_cast<float>(bar.x - 5), 20.0f, MouseButton::Left, false); // outside: nothing
    frame();
    CHECK(tab == 2);
    gui.onMouseUp(static_cast<float>(bar.x - 5), 20.0f, MouseButton::Left);
    // the readout: the idle text, then the parameter under the pointer
    ShadedRenderer idle(400, 120, 2), over(400, 120, 2);
    CGui g1(host, style), g2(host, style);
    g1.setCaching(false);
    g2.setCaching(false);
    auto draw = [&](CGui& g, ShadedRenderer& rr) {
        rr.clear(style.theme.colors.background);
        g.begin(rr, 1.0, 0.016f, 400, 120);
        g.knob(P_out_gain, knob, {.label = "GAIN"});
        g.readout("readout", {100, 90, 200, 20}, {.idle = "IDLE"});
        g.end();
    };
    draw(g1, idle);
    g2.onMouseMove(42.0f, 75.0f);
    draw(g2, over);
    int differ = 0;
    const auto a = idle.frame(), b = over.frame();
    for (int y = 90; y < 110; ++y)
        for (int x = 100; x < 300; ++x)
            differ += at(a, 400, x, y) != at(b, 400, x, y);
    CHECK(differ > 100); // the text in the readout changed
}

TEST_CASE("dropdown: a press opens the list; with drag, a drag steps the value and a click still opens") {
    Host host;
    ClassicStyle style;
    ShadedRenderer r(300, 200, 1);
    CGui gui(host, style);
    gui.setCaching(false);
    const Rect b{20, 100, 120, 24};
    auto frame = [&](bool drag) {
        r.clear(style.theme.colors.background);
        gui.begin(r, 1.0, 0.016f, 300, 200);
        gui.dropdown(P_quality, b, DropdownOptions{.label = "TYPE", .drag = drag, .dragPixels = 10.0f});
        gui.end();
    };
    // plain: a press opens the list
    frame(false);
    gui.onMouseDown(40.0f, 112.0f, MouseButton::Left, false);
    frame(false);
    CHECK(gui.popupOpen());
    gui.onMouseUp(40.0f, 112.0f, MouseButton::Left);
    gui.closePopup();
    frame(false);
    // draggable: dragging up two steps raises the value by two, in one gesture
    host.set(P_quality, 0.0);
    host.drain();
    frame(true);
    gui.onMouseDown(40.0f, 112.0f, MouseButton::Left, false);
    frame(true);
    CHECK_FALSE(gui.popupOpen());
    gui.onMouseMove(40.0f, 91.0f);
    frame(true);
    CHECK(host.plainOf(P_quality) == 2.0);
    gui.onMouseMove(40.0f, 140.0f); // and back down past the start: clamped at the range
    frame(true);
    CHECK(host.plainOf(P_quality) == 0.0);
    gui.onMouseUp(40.0f, 140.0f, MouseButton::Left);
    frame(true);
    CHECK_FALSE(gui.popupOpen());
    const auto kinds = host.drain();
    REQUIRE(kinds.size() >= 3);
    CHECK(kinds.front() == ParamEdit::Kind::Begin);
    CHECK(kinds.back() == ParamEdit::Kind::End);
    // a click without a drag opens the list on release
    gui.onMouseDown(40.0f, 112.0f, MouseButton::Left, false);
    frame(true);
    CHECK_FALSE(gui.popupOpen());
    gui.onMouseUp(40.0f, 112.0f, MouseButton::Left);
    frame(true);
    CHECK(gui.popupOpen());
}

TEST_CASE(
    "preset bar: the arrows step through factory then user presets and wrap, the name opens the list, SAVE reports") {
    Host host;
    ShowPresets presets;
    host.presetHost = &presets;
    ClassicStyle style;
    ShadedRenderer r(400, 80, 1);
    CGui gui(host, style);
    gui.setCaching(false);
    const Rect bar{10, 10, 330, 22};
    PresetBarResult last;
    auto frame = [&] {
        r.clear(style.theme.colors.background);
        gui.begin(r, 1.0, 0.016f, 400, 80);
        last = presetBar(gui, "p", bar);
        gui.end();
    };
    auto click = [&](float x, float y) {
        gui.onMouseDown(x, y, MouseButton::Left, false);
        frame();
        gui.onMouseUp(x, y, MouseButton::Left);
        frame();
    };
    frame();
    CHECK(presets.current == 1);
    const float y = 21.0f;
    click(19.0f, y); // previous
    CHECK(presets.current == 0);
    click(19.0f, y); // wraps to the last, the user's
    CHECK(presets.current == 2);
    const float nextX = static_cast<float>(bar.right() - 44 - 8 - 9); // before SAVE
    click(nextX, y);
    CHECK(presets.current == 0);
    gui.onMouseDown(static_cast<float>(bar.right() - 20), y, MouseButton::Left, false);
    frame();
    CHECK(last.save);
    gui.onMouseUp(static_cast<float>(bar.right() - 20), y, MouseButton::Left);
    frame();
    click(150.0f, y); // the name opens the list
    CHECK(gui.popupOpen());
    // without a preset host there is no bar
    Host bare;
    CGui g2(bare, style);
    g2.setCaching(false);
    r.clear(rgb(1, 2, 3));
    g2.begin(r, 1.0, 0.016f, 400, 80);
    const auto res = presetBar(g2, "p", bar);
    g2.end();
    CHECK_FALSE(res.save);
    CHECK(at(r.frame(), 400, 100, 20) == rgb(1, 2, 3));
}

TEST_CASE("preset bar by category: the name opens the categories, a category opens its presets, a pick loads it") {
    Host host;
    ShowPresets presets;
    host.presetHost = &presets;
    ClassicStyle style;
    ShadedRenderer r(400, 160, 1);
    CGui gui(host, style);
    gui.setCaching(false);
    const Rect bar{10, 10, 330, 22};
    PresetBarOptions opt;
    opt.byCategory = true;
    auto frame = [&] {
        r.clear(style.theme.colors.background);
        gui.begin(r, 1.0, 0.016f, 400, 160);
        presetBar(gui, "p", bar, opt);
        gui.end();
    };
    auto click = [&](float x, float y) {
        gui.onMouseDown(x, y, MouseButton::Left, false);
        frame();
        gui.onMouseUp(x, y, MouseButton::Left);
        frame();
    };
    auto center = [](const Rect& c) { return Point{c.x + c.w * 0.5f, c.y + c.h * 0.5f}; };
    frame();
    CHECK(presets.current == 1);
    click(150.0f, 21.0f); // the name opens the list: Bass, Pad, Lead
    REQUIRE(gui.popupOpen());

    // The lists as the Gui lays them out: the categories under the name button, a submenu beside its row.
    const Rect name{36, 10, 226, 22};
    std::vector<PopupItem> cats = {{"Bass"}, {"Pad"}, {"Lead"}};
    PopupState ps;
    ps.theme = &style.theme;
    ps.items = &cats;
    ps.anchor = name;
    std::vector<Rect> rows;
    const Rect box = style.popup.layout(r, ps, 400, 160, rows);
    REQUIRE(rows.size() == 3);
    std::vector<PopupItem> kids = {{"Hellfire Bass"}};
    PopupState sub = ps;
    sub.items = &kids;
    sub.anchor = Rect{box.right() - 2, rows[0].y - 3, 0, 0};
    std::vector<Rect> kidRows;
    style.popup.layout(r, sub, 400, 160, kidRows);
    REQUIRE(kidRows.size() == 1);

    gui.onMouseMove(center(rows[0]).x, center(rows[0]).y); // Bass opens its list
    frame();
    gui.onMouseMove(center(kidRows[0]).x, center(kidRows[0]).y);
    frame();
    REQUIRE(gui.popupOpen());
    click(center(kidRows[0]).x, center(kidRows[0]).y);
    CHECK_FALSE(gui.popupOpen());
    CHECK(presets.current == 0); // Hellfire Bass
}

TEST_CASE(
    "pick and modulation: a pickable knob and a modulated one differ from a plain one; only arcs show modulation") {
    for (int look = 0; look < 3; ++look) {
        ClassicStyle style(lookConfig(look));
        auto render = [&](bool pick, bool mod) {
            ShadedRenderer r(100, 110, 2);
            r.clear(style.theme.colors.background);
            KnobState s;
            s.theme = &style.theme;
            s.value = 0.4;
            s.pickable = pick;
            s.mod.any = mod;
            s.mod.shared = 0.9;
            style.knob.paint(r, {10, 10, 80, 90}, s);
            return r.frame();
        };
        INFO(lookName(look));
        const auto plain = render(false, false);
        CHECK(diffCount(plain, render(true, false)) > 100);
        const int modDiff = diffCount(plain, render(false, true));
        if (look == 2)
            CHECK(modDiff > 50);
        else
            CHECK(modDiff == 0);
    }
}

// --- the looks -----------------------------------------------------------------------------------------------

TEST_CASE("the config changes the look; configure() applies to the whole style") {
    Host host;
    auto render = [&](ClassicStyle& s) {
        ShadedRenderer r(kPageW, kPageH, 1);
        showcaseFrame(r, host, s);
        return r.frame();
    };
    ClassicStyle slate;
    const auto base = render(slate);
    ClassicStyle silver(ClassicConfig::silver());
    const auto silverFrame = render(silver);
    CHECK(diffCount(base, silverFrame) > 50000);

    ClassicStyle other;
    other.configure(ClassicConfig::silver());
    CHECK(other.theme.colors.accent == ClassicConfig::silver().theme.colors.accent);
    CHECK(diffCount(silverFrame, render(other)) == 0);

    ClassicConfig square = ClassicConfig::slate();
    square.knob.shape = KnobShape::Knurled;
    square.toggle = ToggleShape::Lever;
    ClassicStyle custom(square);
    const auto customFrame = render(custom);
    CHECK(diffCount(base, customFrame) > 1000);
    CHECK(diffCount(silverFrame, customFrame) > 1000);
    // the same config gives the same pixels
    ClassicStyle again;
    CHECK(diffCount(base, render(again)) == 0);
}

TEST_CASE("showcase: deterministic, filled, golden frames in both looks, supersampling levels agree") {
    Host host;
    for (int look = 0; look < 3; ++look) {
        ClassicStyle style(lookConfig(look));
        const std::string name = lookName(look);
        ShadedRenderer r(kPageW, kPageH, 2);
        showcaseFrame(r, host, style);
        const auto a = r.frame();
        ShadedRenderer r2(kPageW, kPageH, 2);
        showcaseFrame(r2, host, style);
        CHECK(diffCount(a, r2.frame()) == 0);
        long lit = 0;
        for (std::uint32_t p : a)
            lit += ui_test::luma(p) > 60;
        CHECK(lit > 15000);
        ui_test::snapshot(name, a, kPageW, kPageH);
        ui_test::checkGolden(name, a, kPageW, kPageH, 0.98);

        ShadedRenderer fast(kPageW, kPageH, 1);
        showcaseFrame(fast, host, style);
        CHECK(ui_test::ssim(fast.frame(), a, kPageW, kPageH) > 0.9);
    }
}

// The widgets as they are drawn today by two plugins' own code, laid out on a canvas. Pixel for pixel the
// painters agree with them (checked outside this repository against those plugins' sources); the golden
// frames keep it so.
namespace {
std::vector<std::uint32_t> slateCompat() {
    constexpr int W = 600, H = 260;
    ClassicConfig cfg = ClassicConfig::slate();
    cfg.knob.topRow = cfg.knob.bottomRow = 0.f;
    cfg.button.radius = 3.f;
    ClassicStyle style(cfg);
    ShadedRenderer r(W, H, 2);
    PanelState ps;
    ps.theme = &style.theme;
    style.panel.paint(r, {0, 0, W, H}, ps);
    const double kv[5] = {0.0, 0.25, 0.5, 0.75, 1.0};
    for (int i = 0; i < 6; ++i) {
        KnobState s;
        s.theme = &style.theme;
        s.value = i < 5 ? kv[i] : 0.6;
        s.hot = i == 5;
        s.showValue = false;
        style.knob.paint(r, {30 + 70 * i - 7, 20 - 7, 58, 58}, s);
    }
    const Argb caps[5] = {0, 0xFF3C9A4E, 0xFFC8322D, 0xFFE8E6DE, 0xFFE7C531};
    const double sv[5] = {0.3, 0.8, 0.5, 0.0, 1.0};
    for (int i = 0; i < 5; ++i) {
        SliderState s;
        s.theme = &style.theme;
        s.vertical = true;
        s.value = sv[i];
        s.hot = i == 2;
        s.accent = caps[i] ? caps[i] : style.theme.colors.accent;
        style.slider.paint(r, {30 + 50 * i, 100, 30, 100}, s);
    }
    for (int i = 0; i < 3; ++i) {
        ToggleState s;
        s.theme = &style.theme;
        s.on = i >= 1;
        s.hot = i == 2;
        s.accent = i == 2 ? 0xFFC8322D : 0xFF2A2B2D;
        style.toggle.paint(r, {(i == 2 ? 380 : 300 + 40 * i) - 4, 96, 28, 48}, s);
    }
    static const char* const kSteps[3] = {"16'", "8'", "4'"};
    SelectorState lever;
    lever.theme = &style.theme;
    lever.vertical = true;
    lever.count = 3;
    lever.index = 1;
    lever.labels = kSteps;
    lever.accent = 0xFFE8E6DE;
    style.selector.paint(r, {440, 100, 50, 100}, lever);
    const Argb tone[4] = {0xFFE9CF3A, 0xFFE9CF3A, 0xFFD23A2E, 0xFF4E8DCF};
    const char* const lab[4] = {"STR\n1", "STR\n1", "BRASS\n1", "CLAV\n1"};
    for (int i = 0; i < 4; ++i) {
        ButtonState s;
        s.theme = &style.theme;
        s.variant = kButtonCap;
        s.on = i == 0 || i == 2;
        s.hot = i == 3;
        s.accent = tone[i];
        s.label = lab[i];
        style.button.paint(r, {30 + 44 * i, 214, 34, 32}, s);
    }
    for (int i = 0; i < 3; ++i) {
        ButtonState s;
        s.theme = &style.theme;
        s.variant = i == 0 ? kButtonArrowLeft : kButtonArrowRight;
        s.hot = i == 1;
        s.active = i == 2;
        style.button.paint(r, {230 + 34 * i, 218, 26, 22}, s);
    }
    return r.frame();
}

std::vector<std::uint32_t> silverCompat() {
    constexpr int W = 600, H = 200, kDivider = 520;
    ClassicStyle style(ClassicConfig::silver());
    ShadedRenderer r(W, H, 2);
    PanelState ps;
    ps.theme = &style.theme;
    style.panel.paint(r, {0, 0, W, H}, ps);
    {
        c::Graphics g = r.graphics();
        c::drawLineAA(g, kDivider + 0.5f, 18.f, kDivider + 0.5f, H - 18.f, 1.6f, 0xF0222326);
    }
    const double kv[6] = {0.0, 0.2, 0.4, 0.6, 0.8, 1.0};
    const Argb caps[6] = {0, 0, 0xFFD23A2E, 0xFFE58B2E, 0xFF4E8DCF, 0};
    for (int i = 0; i < 6; ++i) {
        const int x = 50 + 80 * i, y = 100, rad = 16;
        KnobState s;
        s.theme = &style.theme;
        s.value = kv[i];
        s.label = "CUT OFF";
        s.accent = caps[i] ? caps[i] : style.theme.colors.accent;
        style.knob.paint(r, {x - rad - 15, y - rad - 30, 2 * rad + 31, 2 * rad + 31 + 15}, s);
    }
    for (int i = 0; i < 2; ++i) {
        ToggleState s;
        s.theme = &style.theme;
        s.on = i == 1;
        s.label = "WAVEFORM";
        style.toggle.paint(r, {540 + 30 * i - 40, 110 - 48, 81, 97}, s);
    }
    return r.frame();
}
} // namespace

TEST_CASE("the looks of two plugins, as laid out on their canvases, as golden frames") {
    const auto slate = slateCompat();
    const auto silver = silverCompat();
    ui_test::snapshot("classic_compat_slate", slate, 600, 260);
    ui_test::snapshot("classic_compat_silver", silver, 600, 200);
    ui_test::checkGolden("classic_compat_slate", slate, 600, 260, 0.99);
    // The worn paint of the silver panel is noise made with libm (floor, sin), which differs between toolchains in the
    // last bits and so in the speckle: MSVC measured 0.982 against the GCC frame; the slate canvas has no such noise.
    ui_test::checkGolden("classic_compat_silver", silver, 600, 200, 0.97);
    CHECK(diffCount(slate, slateCompat()) == 0);
    CHECK(diffCount(silver, silverCompat()) == 0);
}

TEST_CASE("the editor shell: cached frames equal uncached frames, and only what changed is redrawn") {
    for (int look = 0; look < 3; ++look) {
        Host host;
        attach(host);
        ClassicStyle style(lookConfig(look));
        ShowRoot root;
        EditorOptions o;
        o.width = kPageW;
        o.height = kPageH;
        Editor<ClassicStyle> ed(host, style, root, o, classicRenderer(2));
        ed.gui().setOverlapCheck(true);
        REQUIRE(ed.frame(false, 0.0));
        INFO(lookName(look));
        CHECK(ed.gui().overlapCount() == 0);
        CHECK(ed.gui().idCollisions() == 0);
        CHECK_FALSE(ed.frame(false, 0.1));

        host.set(P_osc_a_phase_motion_rate, 1.0);
        REQUIRE(ed.frame(false, 0.2));
        CHECK(ed.gui().paintedCount() == 1);
        CHECK(ed.lastDirty().w < kPageW / 2); // one knob and what it overlaps, not the page
        const std::vector<std::uint32_t> cached(ed.pixels(), ed.pixels() + kPageW * kPageH);

        // the same state drawn from scratch
        Host host2;
        host2.set(P_osc_a_phase_motion_rate, 1.0);
        ShadedRenderer r(kPageW, kPageH, 2);
        showcaseFrame(r, host2, style, 0.2);
        const auto fresh = r.frame();
        const int diff = diffCount(cached, fresh);
        INFO("differing pixels " << diff);
        CHECK(diff < kPageW * kPageH / 200);
        CHECK(ui_test::ssim(cached, fresh, kPageW, kPageH) > 0.995);
    }
}
