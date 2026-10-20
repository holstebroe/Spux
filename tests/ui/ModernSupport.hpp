// Shared by ui_modern_test and gpu_renderer_test: a host over the test parameter table, a showcase page
// that uses every standard widget and view of the Modern style, and image helpers for golden frames
// (portable pixmaps, structural similarity).
#pragma once

#include <doctest.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "params/TestParams.hpp"
#include "spux/ui/Gui.hpp"
#include "spux/ui/Layout.hpp"
#include "spux/ui/modern/ModernStyle.hpp"
#include "spux/ui/modern/ModernViews.hpp"
#include "spux/ui/modern/SoftRenderer.hpp"

namespace modern_test {

using namespace spux::ui;
using namespace spux::ui::modern;
using namespace testparams;
using spux::params::EditQueue;
using spux::params::ParamEdit;
using spux::params::ParamStore;

struct Host final : StoreEditorHost<kCount> {
    ParamStore<kCount> store{kTable};
    EditQueue queue;
    int flushes = 0;
    ModulationHost* mod = nullptr;
    Host() : StoreEditorHost(store, queue) {}
    void requestFlush() override { ++flushes; }
    ModulationHost* modulation() override { return mod; }
    double plainOf(std::uint32_t id) { return store.plain(kTable.indexOf(id)); }
    void set(std::uint32_t id, double plain) { store.setPlain(kTable.indexOf(id), plain); }
    std::vector<ParamEdit::Kind> drain() {
        std::vector<ParamEdit::Kind> k;
        ParamEdit e;
        while (queue.pop(e))
            k.push_back(e.kind);
        return k;
    }
};

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

// --- images --------------------------------------------------------------------------------------------

inline bool writePpm(const std::string& path, const std::vector<std::uint32_t>& px, int w, int h) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (std::uint32_t p : px) {
        const unsigned char c[3] = {static_cast<unsigned char>(p >> 16), static_cast<unsigned char>(p >> 8),
                                    static_cast<unsigned char>(p)};
        std::fwrite(c, 1, 3, f);
    }
    std::fclose(f);
    return true;
}

inline bool readPpm(const std::string& path, std::vector<std::uint32_t>& px, int& w, int& h) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    int maxv = 0;
    bool ok = std::fscanf(f, "P6 %d %d %d", &w, &h, &maxv) == 3 && std::fgetc(f) != EOF;
    if (ok) {
        px.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
        for (auto& p : px) {
            unsigned char c[3];
            if (std::fread(c, 1, 3, f) != 3) {
                ok = false;
                break;
            }
            p = 0xFF000000u | (static_cast<std::uint32_t>(c[0]) << 16) | (static_cast<std::uint32_t>(c[1]) << 8) | c[2];
        }
    }
    std::fclose(f);
    return ok;
}

// The mean of each 4 x 4 block: the size of the golden files.
inline std::vector<std::uint32_t> quarter(const std::vector<std::uint32_t>& px, int w, int h) {
    std::vector<std::uint32_t> out(static_cast<std::size_t>(w / 4) * static_cast<std::size_t>(h / 4));
    for (int y = 0; y < h / 4; ++y)
        for (int x = 0; x < w / 4; ++x) {
            unsigned r = 0, g = 0, b = 0;
            for (int dy = 0; dy < 4; ++dy)
                for (int dx = 0; dx < 4; ++dx) {
                    const std::uint32_t p = px[static_cast<std::size_t>(y * 4 + dy) * static_cast<std::size_t>(w) +
                                               static_cast<std::size_t>(x * 4 + dx)];
                    r += (p >> 16) & 255u;
                    g += (p >> 8) & 255u;
                    b += p & 255u;
                }
            out[static_cast<std::size_t>(y) * static_cast<std::size_t>(w / 4) + static_cast<std::size_t>(x)] =
                0xFF000000u | ((r / 16) << 16) | ((g / 16) << 8) | (b / 16);
        }
    return out;
}

// Mean structural similarity over 8 x 8 windows of the luma (standard constants).
inline double ssim(const std::vector<std::uint32_t>& a, const std::vector<std::uint32_t>& b, int w, int h) {
    auto luma = [&](const std::vector<std::uint32_t>& v, int x, int y) {
        const std::uint32_t p =
            v[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)];
        return 0.299 * ((p >> 16) & 255u) + 0.587 * ((p >> 8) & 255u) + 0.114 * (p & 255u);
    };
    const double c1 = 6.5025, c2 = 58.5225;
    double sum = 0;
    int n = 0;
    for (int by = 0; by + 8 <= h; by += 8)
        for (int bx = 0; bx + 8 <= w; bx += 8) {
            double ma = 0, mb = 0, saa = 0, sbb = 0, sab = 0;
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    const double va = luma(a, bx + x, by + y), vb = luma(b, bx + x, by + y);
                    ma += va;
                    mb += vb;
                    saa += va * va;
                    sbb += vb * vb;
                    sab += va * vb;
                }
            ma /= 64;
            mb /= 64;
            const double va = saa / 64 - ma * ma, vb = sbb / 64 - mb * mb, cov = sab / 64 - ma * mb;
            sum += ((2 * ma * mb + c1) * (2 * cov + c2)) / ((ma * ma + mb * mb + c1) * (va + vb + c2));
            ++n;
        }
    return n ? sum / n : 1.0;
}

inline int luma(std::uint32_t p) { return static_cast<int>(((p >> 16) & 255u) + ((p >> 8) & 255u) + (p & 255u)) / 3; }

// Compares a frame with its stored golden (a quarter-size ppm in tests/golden/). SPUX_UPDATE_GOLDEN=1 writes it.
// A missing golden is reported, not failed, so a new case can be added before its file exists.
inline void checkGolden(const std::string& name, const std::vector<std::uint32_t>& frame, int w, int h,
                        double minSimilarity) {
    const std::string path = std::string(SPUX_TEST_DATA_DIR) + "/golden/" + name + ".ppm";
    const auto small = quarter(frame, w, h);
    if (std::getenv("SPUX_UPDATE_GOLDEN")) {
        std::filesystem::create_directories(std::string(SPUX_TEST_DATA_DIR) + "/golden");
        CHECK(writePpm(path, small, w / 4, h / 4));
        return;
    }
    std::vector<std::uint32_t> ref;
    int rw = 0, rh = 0;
    if (!readPpm(path, ref, rw, rh)) {
        std::printf("golden frame %s missing (SPUX_UPDATE_GOLDEN=1 creates it)\n", name.c_str());
        return;
    }
    REQUIRE(rw == w / 4);
    REQUIRE(rh == h / 4);
    const double s = ssim(small, ref, rw, rh);
    INFO(name << " similarity " << s);
    CHECK(s >= minSimilarity);
}

// SPUX_SNAPSHOT_DIR=<dir> writes full-size frames there (to look at, or for documentation screenshots).
inline void snapshot(const std::string& name, const std::vector<std::uint32_t>& frame, int w, int h) {
    if (const char* dir = std::getenv("SPUX_SNAPSHOT_DIR")) {
        std::filesystem::create_directories(dir);
        writePpm(std::string(dir) + "/" + name + ".ppm", frame, w, h);
    }
}

} // namespace modern_test
