// Shared by ui_retro_test: a showcase page that uses every standard widget the Retro style paints, laid out so that it
// reads in all three looks (silver, tracker, forge), and the golden-frame helper for whole-pixel frames.
#pragma once

#include <memory>
#include <vector>

#include "params/TestParams.hpp"
#include "spux/ui/Editor.hpp"
#include "spux/ui/Gui.hpp"
#include "spux/ui/PresetBar.hpp"
#include "spux/ui/retro/RetroStyle.hpp"
#include "ui/GuiTestSupport.hpp"

namespace retro_test {

using namespace spux;
using namespace spux::ui;
using namespace spux::ui::retro;
using namespace testparams;
using ui_test::Host;

using RGui = Gui<RetroStyle>;

// Page size of the golden frames.
inline constexpr int kPageW = 480, kPageH = 352;

// A modulation display for the showcase: the width is modulated up to 0.8.
struct ShowMod final : ModulationHost {
    bool armed() const override { return false; }
    Display display(std::uint32_t id) override {
        Display d;
        if (id == P_width) {
            d.any = true;
            d.base = 0.5;
            d.shared = 0.8;
        }
        return d;
    }
    std::uint32_t depthParam(std::uint32_t) override { return kNone; }
};
inline ShowMod& showMod() {
    static ShowMod m;
    return m;
}

// Three presets for the preset bar.
struct ShowPresets final : PresetHost {
    std::size_t current = 1;
    std::string currentName() override { return names()[current].name; }
    bool modified() override { return true; }
    std::vector<Entry> factory() override { return {names()[0], names()[1]}; }
    std::vector<Entry> user() override { return {names()[2]}; }
    bool load(const Entry& e) override {
        for (std::size_t i = 0; i < 3; ++i)
            if (names()[i].name == e.name)
                current = i;
        return true;
    }
    static const std::vector<Entry>& names() {
        static const std::vector<Entry> n = {{"Hellfire Bass", "Bass", "", 0, true},
                                             {"Glass Pad", "Pad", "", 1, true},
                                             {"My Lead", "Lead", "/u/lead", 0, false}};
        return n;
    }
};

inline const Color kAmber = rgb(232, 150, 40), kBlue = rgb(70, 130, 215), kRed = rgb(214, 58, 46),
                   kCream = rgb(230, 224, 200), kGreen = rgb(60, 168, 90);

// Gives a host what the showcase shows: a modulated parameter and presets.
inline void attach(Host& host) {
    static ShowPresets presets;
    host.mod = &showMod();
    host.presetHost = &presets;
}

// Everything sits 10 pixels below the top, clear of the silver look's trims.
inline Rect R(int x, int y, int w, int h) { return {x, y + 10, w, h}; }

// The surface and the frames (the editor draws them into the background layer).
inline void showcasePanels(RGui& g) {
    g.panel({0, 0, kPageW, kPageH}, "");
    g.panel(R(8, 8, 232, 176), "OSCILLATOR");
    g.panel(R(248, 8, 224, 176), "CONTROLS");
    g.panel(R(8, 192, 464, 112), "SWITCHES");
}

// The widgets: knobs, faders, toggles, selectors, buttons and lamps.
inline void showcaseWidgets(RGui& g) {
    // knobs: plain, bipolar and pickable, a cap color, a modulated one
    g.knob(P_osc_a_phase_motion_rate, R(14, 22, 52, 72), {.label = "RATE"});
    g.knob(P_osc_a_level, R(68, 22, 52, 72), {.label = "LEVEL", .bipolar = true, .pickable = true});
    g.knob(P_width, R(122, 22, 52, 72), {.label = "WIDTH", .accent = kAmber});
    g.knob(P_out_gain, R(176, 22, 52, 72), {.label = "GAIN", .accent = kBlue});
    g.knob(P_env_amp_delay, R(14, 100, 52, 72), {.label = "DELAY", .accent = kRed});
    g.knob(P_transpose, R(68, 100, 52, 72), {.label = "SEMI"});

    // faders: vertical (plain and colored caps), horizontal
    g.slider(P_width, R(252, 22, 38, 156), {.label = "WIDTH", .vertical = true});
    g.slider(P_out_gain, R(292, 22, 38, 156), {.label = "GAIN", .accent = kAmber, .vertical = true});
    g.slider(P_osc_a_level, R(346, 24, 118, 18), {.label = "LEVEL"});
    g.slider(P_env_amp_delay, R(346, 46, 118, 18), {.label = "DELAY", .accent = kBlue});

    // buttons
    g.button("plain", R(334, 72, 62, 16), "PLAIN", {.on = false});
    g.button("plainOn", R(402, 72, 62, 16), "ACTIVE", {.on = true});
    g.button("prev", R(334, 94, 24, 16), "", {.variant = kButtonArrowLeft});
    g.button("next", R(362, 94, 24, 16), "", {.variant = kButtonArrowRight});
    g.button("capOff", R(402, 92, 26, 22), "STR\n1", {.on = false, .accent = kAmber, .variant = kButtonCap});
    g.button("capOn", R(434, 92, 26, 22), "STR\n2", {.on = true, .accent = kAmber, .variant = kButtonCap});
    g.led("ledOff", R(334, 122, 64, 14), false, {.label = "OFF", .accent = kRed});
    g.led("ledOn", R(402, 122, 64, 14), true, {.label = "ON", .accent = kGreen});
    static const char* const kTabs[3] = {"LFO 1", "LFO 2", "MOD"};
    static int tab = 1;
    tab = g.tabs("tabs", R(334, 146, 130, 16), kTabs, 3, tab);

    // toggles and selectors (the same parameter twice, so the second one in an id scope)
    g.toggle(P_mono, R(16, 200, 44, 66), {.label = "MONO"});
    g.pushId("cream");
    g.toggle(P_mono, R(64, 200, 44, 66), {.label = "MONO", .accent = kCream});
    g.popId();
    static const char* const kQuality[3] = {"ECO", "NORM", "HIGH"};
    g.selector(P_quality, R(114, 200, 56, 66), {.labels = kQuality, .accent = kCream, .vertical = true});
    g.pushId("row1");
    g.selector(P_quality, R(178, 204, 150, 20), {.labels = kQuality});
    g.popId();
    g.pushId("row2");
    static const Icon kWaves[3] = {waveIcon(Wave::Sine), waveIcon(Wave::Saw), waveIcon(Wave::Square)};
    g.selector(P_quality, R(178, 230, 150, 20), {.icons = kWaves, .accent = kAmber});
    g.popId();

    // displays and icon buttons
    g.dropdown(P_quality, R(340, 204, 124, 16), "TYPE");
    g.stepper(P_transpose, R(340, 226, 124, 16), "SEMI");
    g.button("tri", R(340, 248, 58, 16), "", {.icon = waveIcon(Wave::Triangle)});
    g.button("noise", R(406, 248, 58, 16), "NOISE", {.on = true, .icon = waveIcon(Wave::Noise)});

    // the keyboard
    static std::uint8_t sounding[128] = {};
    sounding[52] = sounding[56] = sounding[59] = 1;
    g.keyboard("keys", R(178, 262, 150, 34), {.first = 48, .last = 59, .lit = sounding});
    g.keyboard("keys2", R(340, 270, 124, 26), {.first = 60, .last = 71});

    // the preset bar and the readout
    presetBar(g, "presets", R(8, 310, 232, 16));
    g.readout("readout", R(248, 310, 224, 16), {.idle = "TOUCH A CONTROL"});
}

// The showcase as an editor root view: the surface and the frames in the background, widgets every frame.
struct ShowRoot final : RootView<RGui> {
    void background(RGui& g) override { showcasePanels(g); }
    void frame(RGui& g) override { showcaseWidgets(g); }
};

// One frame of the showcase on a renderer, without the editor shell (every widget every frame).
inline void showcaseFrame(IRenderer& r, Host& host, const RetroStyle& style, double time = 1.0) {
    attach(host);
    RGui gui(host, style);
    gui.setCaching(false);
    r.clear(style.theme.colors.background);
    gui.begin(r, time, 0.016f, r.width(), r.height());
    showcasePanels(gui);
    showcaseWidgets(gui);
    gui.end();
}

// The three shipped looks.
inline RetroConfig lookConfig(int look) {
    return look == 0 ? RetroConfig::silver() : look == 1 ? RetroConfig::tracker() : RetroConfig::forge();
}
inline std::string lookName(int look) {
    return look == 0 ? "retro_silver" : look == 1 ? "retro_tracker" : "retro_forge";
}

// Compares a whole-pixel frame with its stored golden (a full-size ppm in tests/golden/): the same size, and at most
// `maxDiffering` of the pixels different. A frame is integer arithmetic, but the knob ticks and arcs take their
// positions from libm, and a toolchain's last bit may move a pixel by one. SPUX_UPDATE_GOLDEN=1 writes the golden. A
// missing or unreadable golden fails, so a deleted file cannot turn the check off.
inline void checkGoldenPixels(const std::string& name, const std::vector<std::uint32_t>& frame, [[maybe_unused]] int w,
                              [[maybe_unused]] int h, [[maybe_unused]] double maxDiffering) {
    const std::string path = std::string(SPUX_TEST_DATA_DIR) + "/golden/" + name + ".ppm";
    if (std::getenv("SPUX_UPDATE_GOLDEN")) {
        std::filesystem::create_directories(std::string(SPUX_TEST_DATA_DIR) + "/golden");
        CHECK(ui_test::writePpm(path, frame, w, h));
        return;
    }
    std::vector<std::uint32_t> ref;
    int rw = 0, rh = 0;
    if (!ui_test::readPpm(path, ref, rw, rh)) {
        INFO("golden frame " << name << " is missing or unreadable (SPUX_UPDATE_GOLDEN=1 creates it)");
        FAIL_CHECK("no golden frame");
        return;
    }
    REQUIRE(rw == w);
    REQUIRE(rh == h);
    [[maybe_unused]] std::size_t differing = 0;
    for (std::size_t i = 0; i < frame.size(); ++i)
        differing += (frame[i] & 0xFFFFFFu) != (ref[i] & 0xFFFFFFu);
    INFO(name << ": " << differing << " of " << frame.size() << " pixels differ");
    CHECK(static_cast<double>(differing) <= maxDiffering * static_cast<double>(frame.size()));
}

} // namespace retro_test
