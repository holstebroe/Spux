// Shared by ui_modern_test and gpu_renderer_test: a showcase page that uses every standard widget and view of
// the Modern style (the host and the image helpers are in GuiTestSupport.hpp).
#pragma once

#include <doctest.h>

#include <cmath>
#include <string>
#include <vector>

#include "params/TestParams.hpp"
#include "spux/ui/Gui.hpp"
#include "spux/ui/Layout.hpp"
#include "spux/ui/modern/ModernStyle.hpp"
#include "spux/ui/modern/ModernViews.hpp"
#include "spux/ui/modern/SoftRenderer.hpp"
#include "ui/GuiTestSupport.hpp"

namespace modern_test {

using namespace spux::ui;
using namespace spux::ui::modern;
using namespace testparams;
using spux::params::EditQueue;
using spux::params::ParamEdit;
using spux::params::ParamStore;

using ui_test::Host;

using MGui = Gui<ModernStyle>;

// One baked font atlas for every software renderer of a test run: baking takes a tenth of a second.
inline std::shared_ptr<const FontAtlas> sharedAtlas() {
    static const std::shared_ptr<const FontAtlas> atlas = SoftRenderer::makeFontAtlas();
    return atlas;
}
// A software renderer with the shared atlas.
inline SoftRenderer makeSoft(int w, int h, int supersample = 2) {
    return SoftRenderer(w, h, supersample, sharedAtlas());
}

// Page size of the golden frames.
inline constexpr int kPageW = 960, kPageH = 560;

// Fixed telemetry for the views, so frames repeat.
struct ShowData {
    std::vector<float> bands, saw, sine;
    float curve[5] = {0.0f, 0.3f, 0.55f, 0.8f, 1.0f};
    CurveEditorState curveState;
    ShowData() {
        for (int i = 0; i < 96; ++i) {
            const float x = static_cast<float>(i) / 95.0f;
            bands.push_back(std::clamp(0.75f * std::exp(-x * 2.2f) + 0.18f * std::sin(x * 31.0f) * std::sin(x * 31.0f) +
                                           0.25f * std::exp(-(x - 0.55f) * (x - 0.55f) * 160.0f),
                                       0.0f, 1.0f));
        }
        for (int i = 0; i < 160; ++i) {
            saw.push_back(std::fmod(static_cast<float>(i) * 0.031f, 1.0f));
            sine.push_back(std::sin(static_cast<float>(i) * 0.09f));
        }
    }
};

inline const Color kBlue = rgb(112, 170, 255), kAmber = rgb(255, 186, 84), kGreen = rgb(118, 226, 164),
                   kViolet = rgb(204, 144, 255), kCyan = rgb(84, 222, 232);

// The showcase's panels (the editor draws them into the background layer).
inline void showcasePanels(MGui& g) {
    g.panel({20, 20, 300, 250}, "Oscillator", kBlue);
    g.panel({340, 20, 280, 250}, "Controls", kAmber);
    g.panel({640, 20, 300, 250}, "Views", kCyan);
    g.panel({20, 290, 400, 250}, "Response", kViolet);
}

// The showcase's widgets: every parameter widget, buttons and the four views.
inline void showcaseWidgets(MGui& g, const ModernStyle& style, ShowData& d) {
    const Color blue = kBlue, amber = kAmber, green = kGreen, violet = kViolet, cyan = kCyan;
    g.setAccent(blue);
    g.knob(P_osc_a_level, {36, 56, 100, 130}, {.large = true});
    g.knob(P_osc_a_phase_motion_rate, {146, 56, 80, 100});
    g.knob(P_width, {232, 56, 80, 100}, {.accent = violet, .bipolar = true});
    g.knob(P_env_amp_delay, {146, 160, 80, 100}, {.accent = amber, .dimmed = true});
    g.knob(P_out_gain, {232, 160, 80, 100}, {.showValue = false});
    g.toggle(P_mono, {40, 200, 90, 24}, {.accent = green});
    static const char* const kQuality[3] = {"Eco", "Norm", "High"};
    g.selector(P_quality, {36, 232, 100, 26}, {.labels = kQuality, .accent = blue});

    g.setAccent(amber);
    g.slider(P_width, {360, 56, 240, 24});
    g.slider(P_out_gain, {360, 90, 240, 24}, {.label = "Gain"});
    g.slider(P_env_amp_delay, {360, 124, 30, 130}, {.vertical = true});
    g.stepper(P_transpose, {410, 124, 190, 28}, "Semitones");
    g.dropdown(P_quality, {410, 164, 190, 28}, "Quality");
    g.button("pill", {410, 204, 90, 26}, "Button", {.on = false});
    g.button("pillOn", {510, 204, 90, 26}, "Active", {.on = true});
    g.button("chip", {410, 238, 90, 24}, "Chip", {.on = false, .variant = kButtonChip});
    g.button("chipOn", {510, 238, 90, 24}, "Chip on", {.on = true, .accent = green, .variant = kButtonChip});

    drawSpectrum(style, g, "spec", {656, 56, 268, 90}, d.bands.data(), static_cast<int>(d.bands.size()), cyan);
    const TraceRow rows[2] = {{"Saw", green, d.saw.data(), static_cast<int>(d.saw.size()), false, 0.42f},
                              {"Sine", violet, d.sine.data(), static_cast<int>(d.sine.size()), true, -0.37f}};
    drawTraces(style, g, "traces", {656, 154, 268, 64}, rows, 2);
    drawStereoMeter(style, g, "meter", {656, 232, 268, 28}, 0.4f, 0.9f);

    drawCurveEditor(style, g, "curve", {32, 322, 376, 210}, "Velocity curve", d.curve, 5, {}, violet, d.curveState);
}

// The showcase as an editor root view: panels in the background, widgets every frame.
struct ShowRoot final : RootView<MGui> {
    const ModernStyle& style;
    ShowData data;
    explicit ShowRoot(const ModernStyle& s) : style(s) {}
    void background(MGui& g) override {
        g.r().drawBackground(kBlue, kViolet, 6.0f);
        showcasePanels(g);
    }
    void frame(MGui& g) override { showcaseWidgets(g, style, data); }
};

// Draws one frame of the showcase on a renderer, without the editor shell (every widget every frame).
inline void showcaseFrame(IRenderer& r, Host& host, const ModernStyle& style, ShowData& d, double time = 6.0) {
    MGui gui(host, style);
    gui.setCaching(false);
    r.clear(style.theme.colors.background);
    r.drawBackground(rgb(112, 170, 255), rgb(204, 144, 255), static_cast<float>(time));
    gui.begin(r, time, 0.016f, r.width(), r.height());
    showcasePanels(gui);
    showcaseWidgets(gui, style, d);
    gui.end();
}

using ui_test::checkGolden;
using ui_test::luma;
using ui_test::quarter;
using ui_test::readPpm;
using ui_test::snapshot;
using ui_test::ssim;
using ui_test::writePpm;

} // namespace modern_test
