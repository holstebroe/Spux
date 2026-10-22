// spux_ui_core: the immediate-mode Gui with a plain test style (interaction, gestures, value entry,
// overlays), the caches (skip, dirty region, erase, equality with an uncached frame), the editor's
// change-driven frames, the telemetry rings and consumers, and the layout helpers.
#include <doctest.h>

#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "params/TestParams.hpp"
#include "spux/core/TelemetryRing.hpp"
#include "spux/ui/Editor.hpp"
#include "spux/ui/Gui.hpp"
#include "spux/ui/Layout.hpp"
#include "spux/ui/Live.hpp"
#include "ui/TestRenderer.hpp"
#include "ui/TestStyle.hpp"

using namespace spux::ui;
using namespace testparams;
using spux::params::EditQueue;
using spux::params::ParamEdit;
using spux::params::ParamStore;

namespace {

struct Host final : StoreEditorHost<kCount> {
    ParamStore<kCount> store{kTable};
    EditQueue queue;
    int flushes = 0;
    ModulationHost* mod = nullptr;
    Host() : StoreEditorHost(store, queue) {}
    // the base keeps references to members constructed after it: re-seat them
    void requestFlush() override { ++flushes; }
    ModulationHost* modulation() override { return mod; }
    double plainOf(std::uint32_t id) { return store.plain(kTable.indexOf(id)); }
    std::vector<ParamEdit::Kind> drain() {
        std::vector<ParamEdit::Kind> k;
        ParamEdit e;
        while (queue.pop(e))
            k.push_back(e.kind);
        return k;
    }
};

using TGui = Gui<uitest::TestStyle>;

struct Rig {
    Host host;
    uitest::TestStyle style;
    TGui gui{host, style};
    uitest::TestRenderer r{240, 160};
    double t = 0.0;
    // One frame like the editor runs it, with a background on the first call.
    void run(const std::function<void(TGui&)>& page) {
        if (first) {
            r.cpu().setTarget(Layer::Background);
            r.clear(rgb(77, 88, 99));
            r.cpu().setTarget(Layer::Widgets);
            r.cpu().restore({0, 0, r.width(), r.height()});
            gui.invalidate();
            first = false;
        } else {
            r.cpu().clearDirty(); // as the editor does after presenting
        }
        gui.begin(r, t, 0.016f, r.width(), r.height());
        page(gui);
        gui.end();
        t += 0.016;
    }
    bool first = true;
};

int diff(const std::vector<std::uint32_t>& a, const std::vector<std::uint32_t>& b) {
    if (a.size() != b.size())
        return -1;
    int n = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        n += a[i] != b[i];
    return n;
}

} // namespace

TEST_CASE("a knob drag is one gesture: begin, values, end; shift makes it fine") {
    Rig f;
    const Rect b{60, 30, 90, 90};
    const double before = f.host.plainOf(P_width);
    auto page = [&](TGui& g) { g.knob(P_width, b); };
    f.run(page);
    f.gui.onMouseMove(100, 70);
    f.gui.onMouseDown(100, 70, MouseButton::Left, false);
    f.run(page);
    f.gui.onMouseMove(100, 20); // 50 pixels up
    f.run(page);
    f.gui.onMouseUp(100, 20, MouseButton::Left);
    f.run(page);
    CHECK(f.host.plainOf(P_width) == doctest::Approx(std::min(1.0, before + 50.0 / 166.0)).epsilon(1e-6));
    const auto kinds = f.host.drain();
    REQUIRE(kinds.size() >= 3);
    CHECK(kinds.front() == ParamEdit::Kind::Begin);
    CHECK(kinds.back() == ParamEdit::Kind::End);
    CHECK(f.host.flushes > 0);

    const double mid = f.host.plainOf(P_width);
    f.t += 1.0; // not a double click
    f.gui.onMouseDown(100, 70, MouseButton::Left, true);
    f.run(page);
    f.gui.onMouseMove(100, 20);
    f.run(page);
    f.gui.onMouseUp(100, 20, MouseButton::Left);
    f.run(page);
    CHECK(f.host.plainOf(P_width) == doctest::Approx(mid - 0.0 + 50.0 * 0.1 / 166.0).epsilon(1e-6));
}

TEST_CASE("double click resets, the wheel steps, a typed value is entered, keys step") {
    Rig f;
    const Rect b{60, 30, 90, 90};
    auto page = [&](TGui& g) { g.knob(P_osc_a_level, b); };
    f.host.store.setPlain(kTable.indexOf(P_osc_a_level), -12.0);
    for (int i = 0; i < 2; ++i) {
        f.gui.onMouseMove(100, 70);
        f.gui.onMouseDown(100, 70, MouseButton::Left, false);
        f.run(page);
        f.gui.onMouseUp(100, 70, MouseButton::Left);
        f.run(page);
    }
    CHECK(f.host.plainOf(P_osc_a_level) == doctest::Approx(0.0));

    f.gui.onWheel(100, 70, 1.0f, false);
    f.run(page);
    CHECK(f.host.plainOf(P_osc_a_level) == doctest::Approx(0.6)); // 2 % of 30 dB

    f.gui.onMouseDown(100, 70, MouseButton::Right, false);
    f.run(page);
    f.gui.onMouseUp(100, 70, MouseButton::Right);
    f.run(page);
    for (int i = 0; i < 12; ++i)
        f.gui.onKey({KeyEvent::Key::Backspace});
    f.run(page);
    KeyEvent k;
    k.key = KeyEvent::Key::Char;
    for (char c : std::string("-6.5")) {
        k.text[0] = c;
        f.gui.onKey(k);
    }
    f.run(page);
    f.gui.onKey({KeyEvent::Key::Enter});
    f.run(page);
    CHECK(f.host.plainOf(P_osc_a_level) == doctest::Approx(-6.5));

    // the knob has the focus from the clicks: Up steps it by the key step
    f.gui.onKey({KeyEvent::Key::Up});
    f.run(page);
    CHECK(f.host.plainOf(P_osc_a_level) == doctest::Approx(-6.5 + 0.6));
}

TEST_CASE("a full edit queue keeps every gesture balanced") {
    Host host; // nothing drains the queue, as with a stalled audio thread
    uitest::TestStyle style;
    TGui gui(host, style);
    for (int i = 0; i < 1500; ++i) {
        gui.beginEdit(P_width);
        for (int k = 0; k < 3; ++k)
            gui.editValue(P_width, 0.1 * k);
        gui.endEdit(P_width);
    }
    int open = 0, begins = 0;
    bool balanced = true;
    ParamEdit e;
    while (host.queue.pop(e)) {
        if (e.kind == ParamEdit::Kind::Begin) {
            balanced &= open == 0;
            ++open;
            ++begins;
        }
        if (e.kind == ParamEdit::Kind::End) {
            balanced &= open == 1;
            --open;
        }
    }
    CHECK(balanced);
    CHECK(open == 0);
    CHECK(begins > 200);
}

TEST_CASE("toggles, two-position switches, selectors, sliders and steppers") {
    Rig f;
    const Rect tb{10, 10, 40, 60}, sb{60, 10, 90, 20}, vb{160, 10, 20, 100}, st{60, 40, 90, 20};
    auto page = [&](TGui& g) {
        g.toggle(P_mono, tb, {.centerBand = 10});
        g.selector(P_quality, sb, {.wheel = 1});
        g.slider(P_width, vb, {.vertical = true, .inverted = true, .dragPixels = 100.0f});
        g.stepper(P_transpose, st);
    };
    f.run(page);
    auto click = [&](float x, float y) {
        f.t += 1.0;
        f.gui.onMouseMove(x, y);
        f.gui.onMouseDown(x, y, MouseButton::Left, false);
        f.run(page);
        f.gui.onMouseUp(x, y, MouseButton::Left);
        f.run(page);
    };
    click(30, 40); // the center band toggles
    CHECK(f.host.plainOf(P_mono) == 1.0);
    click(30, 65); // below the band: on stays on
    CHECK(f.host.plainOf(P_mono) == 1.0);
    click(30, 15); // above: off
    CHECK(f.host.plainOf(P_mono) == 0.0);

    click(140, 20); // the third segment
    CHECK(f.host.plainOf(P_quality) == 2.0);
    f.gui.onWheel(100, 20, -1.0f, false);
    f.run(page);
    CHECK(f.host.plainOf(P_quality) == 1.0);

    // inverted vertical slider: dragging down 20 px raises it by 0.2
    const double w0 = f.host.plainOf(P_width);
    f.t += 1.0;
    f.gui.onMouseDown(170, 50, MouseButton::Left, false);
    f.run(page);
    f.gui.onMouseMove(170, 70);
    f.run(page);
    f.gui.onMouseUp(170, 70, MouseButton::Left);
    f.run(page);
    CHECK(f.host.plainOf(P_width) == doctest::Approx(w0 + 0.2));

    click(145, 50); // the stepper's plus part
    CHECK(f.host.plainOf(P_transpose) == 1.0);
    click(65, 50);
    click(65, 50);
    CHECK(f.host.plainOf(P_transpose) == -1.0);
}

TEST_CASE("widgets draw inside their bounds and their margins") {
    struct Case {
        const char* name;
        std::function<void(TGui&, const Rect&)> draw;
    };
    const Case cases[] = {
        {"knob", [](TGui& g, const Rect& b) { g.knob(P_osc_a_level, b, {.label = "A LONG LABEL TEXT"}); }},
        {"slider", [](TGui& g, const Rect& b) { g.slider(P_width, b); }},
        {"toggle", [](TGui& g, const Rect& b) { g.toggle(P_mono, b); }},
        {"selector", [](TGui& g, const Rect& b) { g.selector(P_quality, b); }},
        {"dropdown", [](TGui& g, const Rect& b) { g.dropdown(P_quality, b, "TYPE"); }},
        {"stepper", [](TGui& g, const Rect& b) { g.stepper(P_transpose, b); }},
        {"button", [](TGui& g, const Rect& b) { g.button("b", b, "A BUTTON WITH A LONG LABEL"); }},
        {"text", [](TGui& g, const Rect& b) { g.text("SOME TEXT THAT IS TOO LONG", b, rgb(255, 255, 255), 11.0f); }},
    };
    for (const Case& c : cases)
        for (Rect b : {Rect{40, 30, 100, 100}, Rect{60, 40, 70, 90}, Rect{50, 50, 150, 30}}) {
            Rig f;
            f.run([&](TGui& g) { c.draw(g, b); });
            const auto px = f.r.frame();
            int outside = 0;
            for (int y = 0; y < f.r.height(); ++y)
                for (int x = 0; x < f.r.width(); ++x)
                    if (!b.contains(x, y) && px[static_cast<std::size_t>(y * f.r.width() + x)] != rgb(77, 88, 99))
                        ++outside;
            INFO(std::string(c.name) << " in " << b.x << "," << b.y << " " << b.w << "x" << b.h);
            CHECK(outside == 0);
        }
}

TEST_CASE("caches: an unchanged widget is skipped, a changed one repaints alone, a gone one is erased") {
    Rig f;
    bool showToggle = true;
    auto page = [&](TGui& g) {
        g.knob(P_width, {10, 10, 60, 60});
        g.knob(P_osc_a_level, {80, 10, 60, 60});
        if (showToggle)
            g.toggle(P_mono, {150, 10, 30, 30});
    };
    f.gui.setOverlapCheck(true);
    f.run(page);
    CHECK(f.gui.paintedCount() == 3);
    f.r.cpu().clearDirty();

    f.run(page); // nothing changed
    CHECK(f.gui.paintedCount() == 0);
    CHECK(f.gui.skippedCount() == 3);
    CHECK(f.r.cpu().dirtyBounds().empty());

    f.host.store.setPlain(kTable.indexOf(P_osc_a_level), -3.0); // host automation
    f.run(page);
    CHECK(f.gui.paintedCount() == 1);
    CHECK(f.r.cpu().dirtyBounds() == Rect{80, 10, 60, 60});
    CHECK(f.gui.overlapCount() == 0);
    CHECK(f.gui.idCollisions() == 0);

    // the cached frame equals one drawn from scratch
    const auto cached = f.r.frame();
    Rig g;
    g.host.store.setPlain(kTable.indexOf(P_osc_a_level), -3.0);
    g.gui.setCaching(false);
    g.run(page);
    CHECK(diff(g.r.frame(), cached) == 0);

    f.r.cpu().clearDirty();
    showToggle = false;
    f.run(page);
    CHECK(f.r.cpu().dirtyBounds() == Rect{150, 10, 30, 30});
    const auto px = f.r.frame();
    CHECK(px[static_cast<std::size_t>(20 * f.r.width() + 160)] == rgb(77, 88, 99));
}

TEST_CASE("overlaps and id collisions are reported") {
    Rig f;
    f.gui.setOverlapCheck(true);
    f.run([](TGui& g) {
        g.knob(P_width, {10, 10, 60, 60});
        g.knob(P_osc_a_level, {50, 10, 60, 60});
        g.knob(P_width, {120, 10, 30, 30});
        for (int i = 0; i < 3; ++i) {
            g.pushId(static_cast<std::uint64_t>(i));
            g.button("same", {10 + 40 * i, 100, 30, 20}, "B");
            g.popId();
        }
    });
    CHECK(f.gui.overlapCount() == 1);
    CHECK(f.gui.idCollisions() == 1);
}

TEST_CASE("a dropdown opens a popup over the widgets; a pick sets the value and the overlay goes") {
    Rig f;
    const Rect d{20, 20, 80, 16};
    auto page = [&](TGui& g) {
        g.dropdown(P_quality, d);
        g.knob(P_width, {20, 40, 60, 60});
    };
    f.run(page);
    const auto before = f.r.frame();
    f.t += 1.0;
    f.gui.onMouseMove(40, 28);
    f.gui.onMouseDown(40, 28, MouseButton::Left, false);
    f.run(page);
    f.gui.onMouseUp(40, 28, MouseButton::Left);
    f.run(page);
    CHECK(f.gui.popupOpen());
    // the third item: below the anchor, 12 px rows
    f.gui.onMouseMove(40, 38 + 2 + 2 * 12 + 4);
    f.run(page);
    // a press on the knob under the list does not reach the knob
    const double w = f.host.plainOf(P_width);
    f.gui.onMouseDown(40, 38 + 2 + 2 * 12 + 4, MouseButton::Left, false);
    f.run(page);
    f.gui.onMouseUp(40, 66, MouseButton::Left);
    f.gui.onMouseMove(150, 150);
    f.run(page);
    CHECK_FALSE(f.gui.popupOpen());
    CHECK(f.host.plainOf(P_quality) == 2.0);
    CHECK(f.host.plainOf(P_width) == w);
    // everything outside the dropdown (which keeps the focus) is as before the list opened
    f.host.store.setPlain(kTable.indexOf(P_quality), 1.0);
    f.run(page);
    const auto after = f.r.frame();
    int changed = 0;
    for (int y = 0; y < f.r.height(); ++y)
        for (int x = 0; x < f.r.width(); ++x)
            if (!d.contains(x, y))
                changed += after[static_cast<std::size_t>(y * f.r.width() + x)] !=
                           before[static_cast<std::size_t>(y * f.r.width() + x)];
    CHECK(changed == 0);
}

namespace {
// A menu with submenus: A, B (B1, B2, B3 (B3a, B3b)), C, D (a disabled row with children).
std::vector<PopupItem> cascade() {
    PopupItem b3{"B3"};
    b3.children = {{"B3a"}, {"B3b"}};
    PopupItem b{"B"};
    b.children = {{"B1"}, {"B2"}, b3};
    PopupItem d{"D", false, false};
    d.children = {{"D1"}};
    return {{"A"}, b, {"C"}, d};
}
} // namespace

TEST_CASE("wheelIn reads the wheel over an area without taking a press") {
    Rig f;
    float seen = 0.0f, outside = 0.0f;
    bool pressed = false;
    const Rect area{20, 20, 80, 16};
    auto page = [&](TGui& g) {
        seen = g.wheelIn(area);
        outside = g.wheelIn({120, 20, 40, 16});
        pressed = g.button("b", area, "x") || pressed;
    };
    f.run(page);
    f.gui.onMouseMove(40, 28);
    f.gui.onWheel(40, 28, 2.0f, false);
    f.run(page);
    CHECK(seen == 2.0f);
    CHECK(outside == 0.0f);
    f.gui.onMouseDown(40, 28, MouseButton::Left, false);
    f.run(page);
    f.gui.onMouseUp(40, 28, MouseButton::Left);
    f.run(page);
    CHECK(pressed); // the button under the same area still takes its press
}

TEST_CASE("a popup with submenus: the pointer opens them beside the row, a pick reports the route") {
    Rig f;
    std::vector<int> route;
    int calls = 0;
    bool open = false;
    const Rect anchor{20, 20, 80, 16};
    auto page = [&](TGui& g) {
        if (open) {
            open = false;
            g.popupPath("menu", anchor, cascade(), -1, [&](const std::vector<int>& r) {
                route = r;
                ++calls;
            });
        }
    };
    open = true;
    f.run(page);
    f.run(page);
    REQUIRE(f.gui.popupOpen());
    // List 0: x 20, y 38, rows 12 high from y 40. The pointer on B (row 1) opens its list: x 98, rows from y 53.
    f.gui.onMouseMove(40, 40 + 12 + 6);
    f.run(page);
    f.gui.onMouseMove(120, 53 + 12 + 6); // B2, in the submenu
    f.run(page);
    CHECK(f.gui.popupOpen());
    f.gui.onMouseDown(120, 53 + 12 + 6, MouseButton::Left, false);
    f.run(page);
    f.gui.onMouseUp(120, 53 + 12 + 6, MouseButton::Left);
    f.run(page);
    CHECK_FALSE(f.gui.popupOpen());
    CHECK(calls == 1);
    CHECK(route == std::vector<int>{1, 1});

    // A third level: B, then B3 in the submenu (row 2 at y 53 + 24), then B3b in its list.
    open = true;
    f.run(page);
    f.run(page);
    f.gui.onMouseMove(40, 40 + 12 + 6);
    f.run(page);
    f.gui.onMouseMove(120, 53 + 24 + 6);
    f.run(page);
    f.gui.onMouseMove(120, 53 + 24 + 6); // B3 opens its list: x 156, rows from y 53 + 24 - 3 + 2 + 2 = 78
    f.run(page);
    f.gui.onMouseMove(170, 78 + 12 + 6); // B3b
    f.run(page);
    f.gui.onMouseDown(170, 78 + 12 + 6, MouseButton::Left, false);
    f.run(page);
    f.gui.onMouseUp(170, 78 + 12 + 6, MouseButton::Left);
    f.run(page);
    CHECK_FALSE(f.gui.popupOpen());
    CHECK(calls == 2);
    CHECK(route == std::vector<int>{1, 2, 1});

    // A press on a row with a submenu keeps the list open and picks nothing; a leaf of the first list picks it.
    open = true;
    f.run(page);
    f.run(page);
    f.gui.onMouseMove(40, 40 + 12 + 6);
    f.gui.onMouseDown(40, 40 + 12 + 6, MouseButton::Left, false);
    f.run(page);
    f.gui.onMouseUp(40, 40 + 12 + 6, MouseButton::Left);
    f.run(page);
    CHECK(f.gui.popupOpen());
    CHECK(calls == 2);
    f.gui.onMouseMove(40, 40 + 24 + 6); // C
    f.run(page);
    f.gui.onMouseDown(40, 40 + 24 + 6, MouseButton::Left, false);
    f.run(page);
    f.gui.onMouseUp(40, 40 + 24 + 6, MouseButton::Left);
    f.run(page);
    CHECK_FALSE(f.gui.popupOpen());
    CHECK(route == std::vector<int>{2});

    // A disabled row opens nothing; a press outside closes the list without a pick; Escape too.
    open = true;
    f.run(page);
    f.run(page);
    f.gui.onMouseMove(40, 40 + 36 + 6); // D, disabled
    f.run(page);
    f.gui.onMouseMove(120, 40 + 36 + 6);
    f.run(page);
    f.gui.onMouseDown(120, 40 + 36 + 6, MouseButton::Left, false); // where D1 would be: outside every list
    f.run(page);
    f.gui.onMouseUp(120, 40 + 36 + 6, MouseButton::Left);
    f.run(page);
    CHECK_FALSE(f.gui.popupOpen());
    CHECK(calls == 3);
    open = true;
    f.run(page);
    f.run(page);
    CHECK(f.gui.popupOpen());
    KeyEvent esc;
    esc.key = KeyEvent::Key::Escape;
    f.gui.onKey(esc);
    f.run(page);
    CHECK_FALSE(f.gui.popupOpen());
    CHECK(calls == 3);
}

TEST_CASE("a submenu that does not fit on the right opens to the left of its list") {
    Rig f;
    std::vector<int> route;
    bool open = false;
    const Rect anchor{150, 20, 80, 16}; // the list ends at 230 of 240
    auto page = [&](TGui& g) {
        if (open) {
            open = false;
            g.popupPath("menu", anchor, cascade(), -1, [&](const std::vector<int>& r) { route = r; });
        }
    };
    open = true;
    f.run(page);
    f.run(page);
    f.gui.onMouseMove(170, 40 + 12 + 6); // B
    f.run(page);
    // Right of the list there is no room for 60 pixels: the submenu is at 150 + 2 - 60 = 92, rows from y 53.
    f.gui.onMouseMove(110, 53 + 6);
    f.run(page);
    f.gui.onMouseDown(110, 53 + 6, MouseButton::Left, false);
    f.run(page);
    f.gui.onMouseUp(110, 53 + 6, MouseButton::Left);
    f.run(page);
    CHECK_FALSE(f.gui.popupOpen());
    CHECK(route == std::vector<int>{1, 0});
}

TEST_CASE("a tooltip shows after the delay and leaves no trace") {
    Rig f;
    auto page = [&](TGui& g) { g.knob(P_width, {20, 20, 60, 60}); };
    f.run(page);
    const auto before = f.r.frame();
    f.gui.onMouseMove(40, 40);
    f.run(page);
    CHECK_FALSE(f.gui.wantsFrame(f.t));
    CHECK(f.gui.wantsFrame(f.t + 1.0));
    f.t += 1.0;
    f.run(page);
    CHECK(diff(f.r.frame(), before) > 0);
    f.gui.onMouseMove(200, 150);
    f.run(page);
    CHECK(diff(f.r.frame(), before) == 0);
}

TEST_CASE("an armed source's depth handle makes the route and drags its depth") {
    struct Mod final : ModulationHost {
        bool armed() const override { return true; }
        Display display(std::uint32_t) override {
            Display d;
            d.base = 0.5;
            return d;
        }
        std::uint32_t depthParam(std::uint32_t target) override {
            requests.push_back(target);
            return P_width;
        }
        std::vector<std::uint32_t> requests;
    } mod;
    Rig f;
    f.host.mod = &mod;
    f.host.store.setPlain(kTable.indexOf(P_width), 0.5);
    const Rect b{20, 20, 100, 60};
    auto page = [&](TGui& g) { g.knob(P_osc_a_level, b); };
    f.run(page);
    // TestKnob puts the handle at y + 10, along the width at the base value 0.5
    const float hx = 20 + 6 + 88 * 0.5f, hy = 30;
    f.gui.onMouseMove(hx, hy);
    f.gui.onMouseDown(hx, hy, MouseButton::Left, false);
    f.run(page);
    f.gui.onMouseMove(hx, hy - 60);
    f.run(page);
    f.gui.onMouseUp(hx, hy - 60, MouseButton::Left);
    f.run(page);
    REQUIRE(mod.requests.size() == 1);
    CHECK(mod.requests[0] == P_osc_a_level);
    CHECK(f.host.plainOf(P_width) == doctest::Approx(0.5 + 60.0 / 120.0 * 0.5));
    CHECK(f.host.plainOf(P_osc_a_level) == 0.0); // the knob itself did not move
}

TEST_CASE("a style without popups and tooltips compiles and runs") {
    Host host;
    uitest::KnobsOnly style;
    Gui<uitest::KnobsOnly> gui(host, style);
    uitest::TestRenderer r(100, 100);
    gui.begin(r, 0.0, 0.0f, 100, 100);
    gui.knob(P_width, {10, 10, 50, 50});
    gui.end();
    CHECK(gui.paintedCount() == 1);
}

TEST_CASE("custom views and interactions") {
    Rig f;
    int draws = 0;
    std::uint64_t key = 1;
    Interaction last;
    auto page = [&](TGui& g) {
        last = g.interact(7, {100, 100, 40, 40});
        if (g.view(7, {100, 100, 40, 40}, key)) {
            ++draws;
            g.r().fillRect({100, 100, 40, 40}, rgb(1, 2, 3));
        }
    };
    f.run(page);
    f.run(page);
    CHECK(draws == 1);
    key = 2;
    f.run(page);
    CHECK(draws == 2);
    f.gui.onMouseDown(110, 110, MouseButton::Left, false);
    f.run(page);
    CHECK(last.pressed);
    f.gui.onMouseMove(130, 105);
    f.run(page);
    CHECK(last.active);
    CHECK(last.dx == 20.0f);
    CHECK(last.dy == -5.0f);
    f.gui.onMouseUp(130, 105, MouseButton::Left);
    f.run(page);
    CHECK(last.released);
}

namespace {
struct CountingRoot final : RootView<TGui> {
    int backgrounds = 0, frames = 0;
    std::uint64_t bgKey = 0, content = 0;
    bool anim = false;
    void background(TGui& g) override {
        ++backgrounds;
        g.panel({0, 0, 50, 50}, "P");
    }
    void frame(TGui& g) override {
        ++frames;
        g.knob(P_width, {60, 10, 60, 60});
    }
    std::uint64_t backgroundKey() const override { return bgKey; }
    std::uint64_t contentVersion() override { return content; }
    bool animating() override { return anim; }
};
} // namespace

TEST_CASE("the editor draws only when something changed") {
    Host host;
    uitest::TestStyle style;
    CountingRoot root;
    EditorOptions o;
    o.width = 200;
    o.height = 100;
    Editor<uitest::TestStyle> ed(host, style, root, o,
                                 [](int w, int h) { return std::make_unique<uitest::TestRenderer>(w, h); });
    CHECK(ed.frame(false, 0.0));
    CHECK(root.backgrounds == 1);
    CHECK(ed.lastDirty() == Rect{0, 0, 200, 100});
    CHECK_FALSE(ed.frame(false, 0.1));
    CHECK_FALSE(ed.frame(false, 0.2));
    host.store.setPlain(kTable.indexOf(P_width), 0.9);
    CHECK(ed.frame(false, 0.3));
    CHECK(ed.lastDirty() == Rect{60, 10, 60, 60});
    CHECK_FALSE(ed.frame(false, 0.4));
    root.content = 5;
    CHECK(ed.frame(false, 0.5));
    root.anim = true;
    CHECK(ed.frame(false, 0.6));
    CHECK(ed.frame(false, 0.7));
    root.anim = false;
    CHECK_FALSE(ed.frame(false, 0.8));
    ed.mouseMove(5, 5);
    CHECK(ed.frame(false, 0.9));
    CHECK(root.backgrounds == 1);
    root.bgKey = 2;
    CHECK(ed.frame(false, 1.0));
    CHECK(root.backgrounds == 2);
    // the pixels: the panel from the background, the knob over it
    CHECK(ed.pixels()[10 * 200 + 10] == rgb(20, 30, 40));
}

TEST_CASE("telemetry rings: frames in order, a full ring drops, off writes nothing; samples") {
    struct Frame {
        int n;
        float v;
    };
    spux::TelemetryRing<Frame, 4> ring;
    CHECK_FALSE(ring.push({1, 0.0f}));
    ring.setEnabled(true);
    for (int i = 0; i < 6; ++i)
        CHECK(ring.push({i, 0.0f}) == (i < 4));
    Frame f{};
    for (int i = 0; i < 4; ++i) {
        REQUIRE(ring.pop(f));
        CHECK(f.n == i);
    }
    CHECK_FALSE(ring.pop(f));
    CHECK(ring.pushed() == 4);

    spux::SampleRing<1024> samples;
    samples.setEnabled(true);
    std::vector<float> l(300), r(300);
    for (int i = 0; i < 300; ++i) {
        l[static_cast<std::size_t>(i)] = static_cast<float>(i);
        r[static_cast<std::size_t>(i)] = static_cast<float>(i);
    }
    samples.push(l.data(), r.data(), 300);
    float out[100];
    REQUIRE(samples.read(out, 100));
    CHECK(out[0] == 200.0f);
    CHECK(out[99] == 299.0f);
    CHECK_FALSE(samples.read(out, 600)); // more than half the ring
}

TEST_CASE("spectrum: a sine lights the band it is in; meters and histories") {
    spux::SampleRing<8192> ring;
    ring.setEnabled(true);
    std::vector<float> s(4096);
    const double hz = 1000.0, sr = 48000.0;
    for (std::size_t i = 0; i < s.size(); ++i)
        s[i] = static_cast<float>(0.5 * std::sin(2.0 * 3.14159265358979323846 * hz * static_cast<double>(i) / sr));
    ring.push(s.data(), nullptr, static_cast<std::uint32_t>(s.size()));
    Spectrum sp;
    for (int i = 0; i < 10; ++i)
        sp.update(ring, sr, 0.016f);
    const auto& b = sp.bands();
    std::size_t best = 0;
    for (std::size_t i = 0; i < b.size(); ++i)
        if (b[i] > b[best])
            best = i;
    const float f0 = sp.frequencyAt(static_cast<float>(best) / static_cast<float>(b.size()));
    const float f1 = sp.frequencyAt(static_cast<float>(best + 1) / static_cast<float>(b.size()));
    CHECK(f0 <= 1000.0f);
    CHECK(f1 >= 1000.0f);
    CHECK(b[best] > 0.8f);

    LevelMeter m;
    m.update(0.5f);
    m.update(0.0f);
    CHECK(m.level() == doctest::Approx(0.45f));
    History<4> h;
    for (int i = 0; i < 6; ++i)
        h.push(static_cast<float>(i));
    CHECK(h.count() == 4);
    CHECK(h[0] == 2.0f);
    CHECK(h[3] == 5.0f);
}

TEST_CASE("layout helpers") {
    constexpr Rect r{10, 20, 100, 50};
    static_assert(rowCell(r, 4, 0) == Rect{10, 20, 25, 50});
    static_assert(rowCell(r, 4, 3, 4) == Rect{10 + 66 + 12, 20, 22, 50});
    static_assert(columnCell(r, 2, 1) == Rect{10, 45, 100, 25});
    static_assert(gridCell(r, 2, 2, 1, 1) == Rect{60, 45, 50, 25});
    static_assert(centerd(r, 20, 10) == Rect{50, 40, 20, 10});
    constexpr auto knobs = pitched<3>(5, 6, 40, 40, 66);
    static_assert(knobs[2] == Rect{137, 6, 40, 40});
    Flow fl(r, 2);
    CHECK(fl.next(30) == Rect{10, 20, 30, 50});
    CHECK(fl.next(10) == Rect{42, 20, 10, 50});
    CHECK(rightPart(r, 30, 5) == Rect{45, 20, 65, 50});
}

TEST_CASE("parameter id 0 is a parameter like any other (hovered, active, tooltip)") {
    static constexpr spux::params::ParamInfo kZero[] = {{0, "zero", "Zero"}, {1, "one", "One"}};
    struct ZeroHost final : EditorHost {
        spux::params::ParamTable table{kZero};
        double values[2] = {0.25, 0.5};
        const spux::params::ParamTable& paramTable() const override { return table; }
        double paramHost(std::size_t i) const override { return values[i]; }
        void setParamHost(std::size_t i, double v) override { values[i] = v; }
        std::uint32_t paramVersion() const override { return 0; }
        bool pushEdit(const ParamEdit&, std::size_t) override { return true; }
        void requestFlush() override {}
    } host;
    uitest::TestStyle style;
    TGui gui(host, style);
    uitest::TestRenderer r(200, 100);
    auto frame = [&](double t) {
        gui.begin(r, t, 0.016f, 200, 100);
        gui.knob(0, {10, 10, 60, 60});
        gui.end();
    };
    frame(0.0);
    CHECK(gui.hoveredParam() == kNoParamId);
    CHECK(gui.activeParam() == kNoParamId);
    gui.onMouseMove(40, 40);
    frame(0.1);
    CHECK(gui.hoveredParam() == 0);
    CHECK(gui.wantsFrame(2.0)); // the tooltip of parameter 0 is due
    gui.onMouseDown(40, 40, MouseButton::Left, false);
    frame(3.0);
    CHECK(gui.activeParam() == 0);
    gui.onMouseMove(40, 20);
    frame(3.1);
    CHECK(host.values[0] > 0.25);
    gui.onMouseUp(40, 20, MouseButton::Left);
    frame(3.2);
    CHECK(gui.activeParam() == kNoParamId);
}
