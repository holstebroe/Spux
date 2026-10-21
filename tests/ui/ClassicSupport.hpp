// Shared by ui_classic_style_test: a showcase page that uses every standard widget the Classic style
// paints, laid out so that it reads in both looks (slate and silver).
#pragma once

#include <memory>
#include <vector>

#include "params/TestParams.hpp"
#include "spux/ui/Editor.hpp"
#include "spux/ui/Gui.hpp"
#include "spux/ui/classic/ClassicStyle.hpp"
#include "ui/GuiTestSupport.hpp"

namespace classic_test {

using namespace spux;
using namespace spux::ui;
using namespace spux::ui::classic;
using namespace testparams;
using ui_test::Host;

using CGui = Gui<ClassicStyle>;

// Page size of the golden frames.
inline constexpr int kPageW = 640, kPageH = 400;

inline const Argb kAmber = rgb(232, 150, 40), kBlue = rgb(70, 130, 215), kRed = rgb(214, 58, 46),
                  kCream = rgb(230, 224, 200), kGreen = rgb(60, 168, 90);

// The surface and the frames (the editor draws them into the background layer).
inline void showcasePanels(CGui& g) {
    g.panel({0, 0, kPageW, kPageH}, "");
    g.panel({12, 12, 306, 196}, "OSCILLATOR");
    g.panel({326, 12, 302, 196}, "CONTROLS");
    g.panel({12, 216, 616, 172}, "SWITCHES");
}

// The widgets: knobs, faders, toggles, selectors, buttons and lamps.
inline void showcaseWidgets(CGui& g) {
    // knobs: plain, bipolar, a cap color, a different value
    g.knob(P_osc_a_phase_motion_rate, {22, 40, 64, 80}, {.label = "RATE"});
    g.knob(P_osc_a_level, {92, 40, 64, 80}, {.label = "LEVEL", .bipolar = true});
    g.knob(P_width, {162, 40, 64, 80}, {.label = "WIDTH", .accent = kAmber});
    g.knob(P_out_gain, {232, 40, 64, 80}, {.label = "GAIN", .accent = kBlue});
    g.knob(P_env_amp_delay, {22, 124, 64, 80}, {.label = "DELAY", .accent = kRed});
    g.knob(P_transpose, {92, 124, 64, 80}, {.label = "SEMI"});

    // faders: vertical (dark and colored caps), horizontal
    g.slider(P_width, {338, 28, 44, 168}, {.label = "WIDTH", .vertical = true});
    g.slider(P_out_gain, {394, 28, 44, 168}, {.label = "GAIN", .accent = kAmber, .vertical = true});
    g.slider(P_osc_a_level, {450, 36, 168, 26}, {.label = "LEVEL"});
    g.slider(P_env_amp_delay, {450, 70, 168, 26}, {.label = "DELAY", .accent = kBlue});

    // buttons
    g.button("plain", {450, 106, 74, 22}, "PLAIN", {.on = false});
    g.button("plainOn", {534, 106, 74, 22}, "ACTIVE", {.on = true});
    g.button("prev", {450, 140, 36, 22}, "", {.variant = kButtonArrowLeft});
    g.button("next", {494, 140, 36, 22}, "", {.variant = kButtonArrowRight});
    g.button("capOff", {540, 140, 30, 30}, "STR\n1", {.on = false, .accent = kAmber, .variant = kButtonCap});
    g.button("capOn", {578, 140, 30, 30}, "STR\n2", {.on = true, .accent = kAmber, .variant = kButtonCap});
    g.led("ledOff", {450, 184, 76, 22}, false, {.label = "OFF", .accent = kRed});
    g.led("ledOn", {544, 184, 76, 22}, true, {.label = "ON", .accent = kGreen});

    // toggles and selectors (the same parameter twice, so the second one in an id scope)
    // a lever wants room for its plate and label, a rocker is a small key
    const bool lever = g.style().config().toggle == ToggleShape::Lever;
    const Rect t1 = lever ? Rect{20, 226, 62, 80} : Rect{34, 240, 32, 52};
    const Rect t2 = lever ? Rect{92, 226, 62, 80} : Rect{96, 240, 32, 52};
    g.toggle(P_mono, t1, {.label = "MONO"});
    g.pushId("cream");
    g.toggle(P_mono, t2, {.label = "MONO", .accent = kCream});
    g.popId();
    static const char* const kQuality[3] = {"ECO", "NORM", "HIGH"};
    g.selector(P_quality, {170, 230, 80, 74}, {.labels = kQuality, .accent = kCream, .vertical = true});
    g.pushId("row1");
    g.selector(P_quality, {270, 236, 200, 28}, {.labels = kQuality});
    g.popId();
    g.pushId("row2");
    static const Icon kWaves[3] = {waveIcon(Wave::Sine), waveIcon(Wave::Saw), waveIcon(Wave::Square)};
    g.selector(P_quality, {270, 276, 200, 28}, {.icons = kWaves, .accent = kAmber});
    g.popId();

    // displays, icon buttons and the keyboard
    g.dropdown(P_quality, {484, 232, 130, 24}, "TYPE");
    g.stepper(P_transpose, {484, 264, 130, 24}, "SEMI");
    g.button("tri", {484, 300, 60, 24}, "", {.icon = waveIcon(Wave::Triangle)});
    g.button("noise", {554, 300, 60, 24}, "NOISE", {.on = true, .icon = waveIcon(Wave::Noise)});
    static std::uint8_t sounding[128] = {};
    sounding[52] = sounding[56] = sounding[59] = 1;
    g.keyboard("keys", {24, 318, 440, 60}, {.first = 48, .last = 71, .lit = sounding});
}

// The showcase as an editor root view: the surface and the frames in the background, widgets every frame.
struct ShowRoot final : RootView<CGui> {
    void background(CGui& g) override { showcasePanels(g); }
    void frame(CGui& g) override { showcaseWidgets(g); }
};

// One frame of the showcase on a renderer, without the editor shell (every widget every frame).
inline void showcaseFrame(IRenderer& r, Host& host, const ClassicStyle& style, double time = 1.0) {
    CGui gui(host, style);
    gui.setCaching(false);
    r.clear(style.theme.colors.background);
    gui.begin(r, time, 0.016f, r.width(), r.height());
    showcasePanels(gui);
    showcaseWidgets(gui);
    gui.end();
}

} // namespace classic_test
