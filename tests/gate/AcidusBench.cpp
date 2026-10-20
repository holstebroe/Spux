// Acceptance gate: CPU per frame of the Acidus panel and a pixel check, built twice from the same source:
// against Acidus's retained GuiWindow and against the immediate-mode port (tests/gate/acidus). The
// scenarios are those of Acidus's own gui_perf_test: idle (the logo animates), a knob dragged, every knob
// automated. Each timed step is the input and the frame it causes, as the window's thread runs them.
//
//   acidus_gate_bench_<variant>            timings (median and mean per frame, best of 5 runs)
//   acidus_gate_bench_<variant> --<scenario> N   N frames of one scenario, for a profiler
//   acidus_gate_bench_<variant> --pixels   a frozen, scripted session: prints the frame hash after each
//                                          step (both builds must print the same) and writes PPMs to a dir
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "clap/AcidusClap.hpp"
#include "gui/GuiWindow.hpp"

using clk = std::chrono::steady_clock;
using acidus::AcidusClap;
using acidus::GuiWindow;

namespace {

struct Stats {
    double median, mean;
};

Stats timeFrames(int n, const std::function<void(int)>& frame) {
    Stats best{1e9, 1e9};
    for (int run = 0; run < 5; ++run) {
        for (int i = 0; i < 3; ++i)
            frame(i);
        std::vector<double> ms;
        for (int i = 0; i < n; ++i) {
            const auto t0 = clk::now();
            frame(i + 3);
            ms.push_back(std::chrono::duration<double, std::milli>(clk::now() - t0).count());
        }
        std::sort(ms.begin(), ms.end());
        best.median = std::min(best.median, ms[ms.size() / 2]);
        best.mean = std::min(best.mean, std::accumulate(ms.begin(), ms.end(), 0.0) / static_cast<double>(n));
    }
    return best;
}

void report(const char* name, Stats s) {
    std::printf("  %-40s median %8.4f ms  mean %8.4f ms\n", name, s.median, s.mean);
}

std::uint64_t hashPixels(const std::vector<uint32_t>& px) {
    std::uint64_t h = 1469598103934665603ull;
    for (uint32_t p : px)
        for (int b = 0; b < 4; ++b) {
            h ^= (p >> (8 * b)) & 0xFFu;
            h *= 1099511628211ull;
        }
    return h;
}

void writePpm(const GuiWindow& w, const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f)
        return;
    std::fprintf(f, "P6\n%u %u\n255\n", w.getWidth(), w.getHeight());
    for (uint32_t px : w.getPixelBuffer()) {
        const unsigned char rgb[3] = {static_cast<unsigned char>(px >> 16), static_cast<unsigned char>(px >> 8),
                                      static_cast<unsigned char>(px)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
}

int pixels(const char* dir) {
    AcidusClap plugin(nullptr);
    GuiWindow gui(&plugin);
    gui.setAnimationFrozen(true);
    int step = 0;
    auto shot = [&](const char* what) {
        gui.renderFrame();
        std::printf("%-28s %016llx\n", what, static_cast<unsigned long long>(hashPixels(gui.getPixelBuffer())));
        if (dir)
            writePpm(gui, std::string(dir) + "/acidus_" + std::to_string(step) + ".ppm");
        ++step;
    };
    shot("first frame");
    for (int i = 0; i < 30; ++i)
        gui.advanceAnimation(1.0 / 30.0);
    shot("animated 1 s");
    gui.handleMouseDown(120, 100, false);
    gui.renderFrame();
    gui.handleMouseDrag(120, 70, false);
    shot("cutoff dragged up 30 px");
    gui.handleMouseUp();
    gui.renderFrame();
    gui.handleMouseDown(50, 115, false);
    shot("waveform switched");
    gui.handleMouseUp();
    gui.renderFrame();
    gui.handleMouseDown(594, 100, true);
    gui.renderFrame();
    gui.handleMouseDrag(594, 40, true);
    shot("drive, fine drag");
    gui.handleMouseUp();
    gui.renderFrame();
    plugin.setParamValueFromGui(acidus::PARAM_TUNE, 350.0);
    shot("tuning automated");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--pixels") == 0)
        return pixels(argc > 2 ? argv[2] : nullptr);
    if (argc > 2) { // one scenario, N frames, for a profiler: --idle | --drag | --automation N
        const int n = std::atoi(argv[2]);
        const std::string what = argv[1];
        AcidusClap plugin(nullptr);
        GuiWindow gui(&plugin);
        gui.renderFrame();
        if (what == "--drag") {
            gui.handleMouseDown(120, 100, false);
            gui.renderFrame();
        }
        for (int i = 0; i < n; ++i) {
            if (what == "--idle")
                gui.advanceAnimation(1.0 / 60.0);
            if (what == "--drag")
                gui.handleMouseDrag(120, 100 - (i % 60), false);
            if (what == "--automation")
                for (clap_id id :
                     {acidus::PARAM_CUTOFF, acidus::PARAM_RESONANCE, acidus::PARAM_ENV_MOD, acidus::PARAM_DECAY,
                      acidus::PARAM_ACCENT, acidus::PARAM_VOLUME, acidus::PARAM_DRIVE})
                    plugin.setParamValueFromGui(id, ((i * 7 + static_cast<int>(id) * 13) % 100) / 100.0);
            gui.renderFrame();
        }
        return 0;
    }
    std::printf("Acidus panel, %s, %s (software, no window)\n", ACIDUS_GATE_VARIANT, ACIDUS_GATE_STYLE);
    AcidusClap plugin(nullptr);
    std::unique_ptr<GuiWindow> gui;
    {
        const auto t0 = clk::now();
        gui = std::make_unique<GuiWindow>(&plugin);
        gui->renderFrame();
        std::printf("  %-40s %8.3f ms\n", "first frame (window open)",
                    std::chrono::duration<double, std::milli>(clk::now() - t0).count());
    }
    report("idle (logo animation)", timeFrames(240, [&](int) {
               gui->advanceAnimation(1.0 / 60.0);
               gui->renderFrame();
           }));
    gui->handleMouseDown(120, 100, false); // the Cut Off Freq knob
    gui->renderFrame();
    report("knob drag (value changes)", timeFrames(240, [&](int i) {
               gui->handleMouseDrag(120, 100 - (i % 60), false);
               gui->renderFrame();
           }));
    gui->handleMouseUp();
    gui->renderFrame();
    report("automation (all knobs change)", timeFrames(120, [&](int i) {
               for (clap_id id : {acidus::PARAM_CUTOFF, acidus::PARAM_RESONANCE, acidus::PARAM_ENV_MOD,
                                  acidus::PARAM_DECAY, acidus::PARAM_ACCENT, acidus::PARAM_VOLUME, acidus::PARAM_DRIVE})
                   plugin.setParamValueFromGui(id, ((i * 7 + static_cast<int>(id) * 13) % 100) / 100.0);
               gui->renderFrame();
           }));
    return 0;
}
