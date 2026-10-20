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
    for (int look = 0; look < 2; ++look) {
        ClassicStyle style(look ? ClassicConfig::silver() : ClassicConfig::slate());
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
    CHECK(r.calls == 0);
}

TEST_CASE("the lamp: lit is brighter than unlit and glows beyond its lens") {
    auto render = [](bool lit) {
        ShadedRenderer r(60, 60, 2);
        r.clear(rgb(40, 40, 44));
        drawLed(r, {30.0f, 30.0f}, 4.0f, rgb(255, 40, 30), lit);
        return r.frame();
    };
    const auto off = render(false), on = render(true);
    CHECK(((at(on, 60, 30, 30) >> 16) & 255u) > ((at(off, 60, 30, 30) >> 16) & 255u) + 80);
    CHECK(((at(on, 60, 30, 37) >> 16) & 255u) > ((at(off, 60, 30, 37) >> 16) & 255u)); // the halo
    CHECK(at(on, 60, 30, 48) == rgb(40, 40, 44));                                      // and it ends
    CHECK(diffCount(on, render(true)) == 0);
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
    for (int look = 0; look < 2; ++look) {
        ClassicStyle style(look ? ClassicConfig::silver() : ClassicConfig::slate());
        const std::string name = look ? "classic_silver" : "classic_slate";
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
    ui_test::checkGolden("classic_compat_silver", silver, 600, 200, 0.99);
    CHECK(diffCount(slate, slateCompat()) == 0);
    CHECK(diffCount(silver, silverCompat()) == 0);
}

TEST_CASE("the editor shell: cached frames equal uncached frames, and only what changed is redrawn") {
    for (int look = 0; look < 2; ++look) {
        Host host;
        ClassicStyle style(look ? ClassicConfig::silver() : ClassicConfig::slate());
        ShowRoot root;
        EditorOptions o;
        o.width = kPageW;
        o.height = kPageH;
        Editor<ClassicStyle> ed(host, style, root, o, classicRenderer(2));
        ed.gui().setOverlapCheck(true);
        REQUIRE(ed.frame(false, 0.0));
        INFO((look ? "silver" : "slate"));
        CHECK(ed.gui().overlapCount() == 0);
        CHECK(ed.gui().idCollisions() == 0);
        CHECK_FALSE(ed.frame(false, 0.1));

        host.set(P_transpose, 7.0);
        REQUIRE(ed.frame(false, 0.2));
        CHECK(ed.gui().paintedCount() == 1);
        CHECK(ed.lastDirty().w < kPageW / 2); // one knob and what it overlaps, not the page
        const std::vector<std::uint32_t> cached(ed.pixels(), ed.pixels() + kPageW * kPageH);

        // the same state drawn from scratch
        Host host2;
        host2.set(P_transpose, 7.0);
        ShadedRenderer r(kPageW, kPageH, 2);
        showcaseFrame(r, host2, style, 0.2);
        const auto fresh = r.frame();
        const int diff = diffCount(cached, fresh);
        INFO("differing pixels " << diff);
        CHECK(diff < kPageW * kPageH / 200);
        CHECK(ui_test::ssim(cached, fresh, kPageW, kPageH) > 0.995);
    }
}
