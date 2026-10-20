// Acceptance gate: CPU per frame of the Tannhauser panel and a pixel check, built twice from the same
// source: against Tannhauser's retained GuiWindow and against the immediate-mode port
// (tests/gate/tannhauser). Each timed step is the input and the frame it causes.
//
//   tannhauser_gate_bench_<variant>_classic            timings (median and mean per frame, best of 5 runs)
//   tannhauser_gate_bench_<variant>_classic --pixels   a scripted session: the frame hash after each step
//                                                      (both builds must print the same); PPMs to a dir
//   tannhauser_gate_bench_<variant>_classic --<scenario> N   N frames of one scenario, for a profiler
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <numeric>
#include <string>
#include <vector>

#include "PanelLayout.hpp"
#include "clap/TannhauserClap.hpp"
#include "gui/GuiWindow.hpp"

using clk = std::chrono::steady_clock;
using namespace tannhauser;

namespace {

const clap_host_t kHost = {
    CLAP_VERSION,
    nullptr,
    "gate",
    "spux",
    "",
    "1.0",
    [](const clap_host_t*, const char*) -> const void* { return nullptr; },
    [](const clap_host_t*) {},
    [](const clap_host_t*) {},
    [](const clap_host_t*) {},
};

struct Rig {
    const clap_plugin_t* cp = nullptr;
    TannhauserClap* p = nullptr;
    GuiWindow* gui = nullptr;
    Rig() {
        const auto* factory = static_cast<const clap_plugin_factory_t*>(clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
        cp = factory->create_plugin(factory, &kHost, "com.holstebroe.tannhauser");
        p = static_cast<TannhauserClap*>(cp->plugin_data);
        cp->init(cp);
        cp->activate(cp, 48000.0, 32, 1024);
        p->createGuiWindow();
        gui = p->getGuiWindow();
    }
    ~Rig() {
        p->destroyGuiWindow();
        cp->deactivate(cp);
        cp->destroy(cp);
    }
};

const Ctl& first(CtlType t, int skip = 0) {
    for (const Ctl& c : panelLayout().controls)
        if (c.type == t && skip-- == 0)
            return c;
    std::abort();
}

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

// The scenarios, each one step (input and frame) per call.
struct Scenarios {
    Rig& r;
    const Ctl& slider = first(CtlType::Slider, 8); // line I VCF: LPF
    const Ctl& knob = first(CtlType::Knob);        // VOLUME
    const Ctl& rocker = first(CtlType::Rocker);
    void idle(int) { r.gui->renderFrame(); }
    void hover(int i) {
        const Ctl& c = (i & 1) ? slider : knob;
        r.gui->handleMouseMove(c.x + c.w / 2, c.y + c.h / 2);
        r.gui->renderFrame();
    }
    void beginDrag(const Ctl& c) {
        r.gui->handleMouseMove(c.x + c.w / 2, c.y + c.h / 2);
        r.gui->renderFrame();
        r.gui->handleMouseDown(c.x + c.w / 2, c.y + c.h / 2, false);
        r.gui->renderFrame();
    }
    void drag(const Ctl& c, int i) {
        r.gui->handleMouseDrag(c.x + c.w / 2, c.y + c.h / 2 - (i % 40), false);
        r.gui->renderFrame();
    }
    void endDrag() {
        r.gui->handleMouseUp();
        r.gui->renderFrame();
    }
    void automation(int i) {
        for (uint32_t k = 0; k < 10; ++k) {
            const uint32_t id = lineParam(static_cast<int>(k & 1), static_cast<LineParam>(LP_HPF + k));
            r.p->onParamValueFromGui(id, ((i * 7 + static_cast<int>(id) * 13) % 100) / 100.0);
        }
        r.gui->renderFrame();
    }
    void notes(int i) {
        r.p->guiNote(48 + (i % 24), 0.8, (i / 24) % 2 == 0);
        r.gui->renderFrame();
    }
};

std::uint64_t hashPixels(const std::vector<uint32_t>& px) {
    std::uint64_t h = 1469598103934665603ull;
    for (uint32_t p : px)
        for (int b = 0; b < 4; ++b) {
            h ^= (p >> (8 * b)) & 0xFFu;
            h *= 1099511628211ull;
        }
    return h;
}

// Every input is followed by a frame, as the retained window draws inside its input handlers: both
// variants then redraw at the same moments.
int pixels(const char* dir) {
    Rig r;
    Scenarios s{r};
    int step = 0;
    auto shot = [&](const char* what) {
        r.gui->renderFrame();
        std::printf("%-34s %016llx  %s\n", what, static_cast<unsigned long long>(hashPixels(r.gui->getPixelBuffer())),
                    r.gui->readoutText().substr(0, r.gui->readoutText().find('\n')).c_str());
        if (dir) {
            FILE* f = std::fopen((std::string(dir) + "/tannhauser_" + std::to_string(step) + ".ppm").c_str(), "wb");
            if (f) {
                std::fprintf(f, "P6\n%u %u\n255\n", r.gui->getWidth(), r.gui->getHeight());
                for (uint32_t px : r.gui->getPixelBuffer()) {
                    const unsigned char rgb[3] = {static_cast<unsigned char>(px >> 16),
                                                  static_cast<unsigned char>(px >> 8), static_cast<unsigned char>(px)};
                    std::fwrite(rgb, 1, 3, f);
                }
                std::fclose(f);
            }
        }
        ++step;
    };
    shot("first frame");
    r.gui->handleMouseMove(s.slider.x + s.slider.w / 2, s.slider.y + 10);
    r.gui->renderFrame();
    shot("hover a slider");
    s.beginDrag(s.slider);
    s.drag(s.slider, 25);
    shot("drag it up 25 px");
    s.drag(s.slider, 35);
    shot("further");
    s.endDrag();
    const Ctl& paddle = first(CtlType::Paddle, 2);
    s.beginDrag(paddle);
    s.drag(paddle, 20);
    shot("paddle dragged");
    s.endDrag();
    r.gui->handleMouseMove(s.rocker.x + s.rocker.w / 2, s.rocker.y + s.rocker.h / 2);
    r.gui->renderFrame();
    r.gui->handleMouseDown(s.rocker.x + s.rocker.w / 2, s.rocker.y + s.rocker.h / 2, false);
    r.gui->renderFrame();
    r.gui->handleMouseUp();
    r.gui->renderFrame();
    shot("rocker clicked");
    const Ctl& lever = first(CtlType::Lever);
    r.gui->handleMouseMove(lever.x + lever.w / 2, lever.y + lever.h - 10);
    r.gui->renderFrame();
    r.gui->handleMouseDown(lever.x + lever.w / 2, lever.y + lever.h - 10, false);
    r.gui->renderFrame();
    r.gui->handleMouseUp();
    r.gui->renderFrame();
    shot("lever set low");
    r.gui->handleWheel(lever.x + lever.w / 2, lever.y + lever.h - 10, 1);
    r.gui->renderFrame();
    shot("lever wheeled up");
    const Ctl& tone = first(CtlType::ToneButton, 3);
    r.gui->handleMouseMove(tone.x + tone.w / 2, tone.y + tone.h / 2);
    r.gui->renderFrame();
    r.gui->handleMouseDown(tone.x + tone.w / 2, tone.y + tone.h / 2, false);
    r.gui->renderFrame();
    r.gui->handleMouseUp();
    r.gui->renderFrame();
    shot("factory tone loaded");
    s.beginDrag(s.knob);
    s.drag(s.knob, 30);
    shot("volume knob");
    s.endDrag();
    const Ctl& kb = first(CtlType::Keyboard);
    r.gui->handleMouseMove(kb.x + 100, kb.y + 100);
    r.gui->renderFrame();
    r.gui->handleMouseDown(kb.x + 100, kb.y + 100, false);
    r.gui->renderFrame();
    shot("key held");
    r.gui->handleMouseUp();
    r.gui->renderFrame();
    shot("key released");
    const Ctl& ribbon = first(CtlType::Ribbon);
    r.gui->handleMouseMove(ribbon.x + 300, ribbon.y + 10);
    r.gui->renderFrame();
    r.gui->handleMouseDown(ribbon.x + 300, ribbon.y + 10, false);
    r.gui->renderFrame();
    r.gui->handleMouseDrag(ribbon.x + 400, ribbon.y + 10, false);
    r.gui->renderFrame();
    shot("ribbon bent");
    r.gui->handleMouseUp();
    r.gui->renderFrame();
    shot("ribbon released");
    s.automation(5);
    shot("automation");
    r.gui->handleMouseMove(5, 600);
    r.gui->renderFrame();
    shot("pointer away");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--pixels") == 0)
        return pixels(argc > 2 ? argv[2] : nullptr);
    Rig r;
    Scenarios s{r};
    if (argc > 2) { // one scenario, N frames, for a profiler
        const int n = std::atoi(argv[2]);
        const std::string what = argv[1];
        r.gui->renderFrame();
        if (what == "--idle")
            for (int i = 0; i < n; ++i)
                s.idle(i);
        if (what == "--hover")
            for (int i = 0; i < n; ++i)
                s.hover(i);
        if (what == "--drag") {
            s.beginDrag(s.slider);
            for (int i = 0; i < n; ++i)
                s.drag(s.slider, i);
            s.endDrag();
        }
        if (what == "--automation")
            for (int i = 0; i < n; ++i)
                s.automation(i);
        return 0;
    }
    std::printf("Tannhauser panel, %s, classic (software, no window)\n", TANNHAUSER_GATE_VARIANT);
    {
        const auto t0 = clk::now();
        r.gui->renderFrame();
        std::printf("  %-40s %8.3f ms\n", "first frame",
                    std::chrono::duration<double, std::milli>(clk::now() - t0).count());
    }
    auto report = [](const char* name, Stats st) {
        std::printf("  %-40s median %8.4f ms  mean %8.4f ms\n", name, st.median, st.mean);
    };
    report("idle (nothing changed)", timeFrames(240, [&](int i) { s.idle(i); }));
    report("hover (pointer moves between controls)", timeFrames(240, [&](int i) { s.hover(i); }));
    s.beginDrag(s.slider);
    report("slider drag (value changes)", timeFrames(240, [&](int i) { s.drag(s.slider, i); }));
    s.endDrag();
    s.beginDrag(s.knob);
    report("knob drag (value changes)", timeFrames(240, [&](int i) { s.drag(s.knob, i); }));
    s.endDrag();
    report("automation (10 sliders change)", timeFrames(120, [&](int i) { s.automation(i); }));
    report("host notes (keyboard redraws)", timeFrames(120, [&](int i) { s.notes(i); }));
    return 0;
}
