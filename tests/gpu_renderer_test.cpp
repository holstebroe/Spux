// The GPU renderer in a headless GL context: structure, not exact pixels (driver differences must pass).
// Needs an X display for the context (xvfb-run) and OpenGL 3.3; every case is skipped without them.
// The showcase page is compared with a stored golden frame and with the software renderer's frame.
#include <doctest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "spux/gl/GlContext.hpp"
#include "spux/ui/modern/GpuRenderer.hpp"
#include "spux/ui/modern/ModernSurface.hpp"
#include "spux/ui/modern/SoftRenderer.hpp"
#include "ui/ModernSupport.hpp"

#if defined(SPUX_HAS_X11)
#include <X11/Xlib.h> // after spux: Xlib defines macros such as Bool
#include <X11/Xutil.h>
#endif

using namespace modern_test;

namespace {

std::uint32_t at(const std::vector<std::uint32_t>& px, int w, int x, int y) {
    return px[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)];
}

struct Gpu {
    std::unique_ptr<gl::GlContext> ctx;
    gl::GlApi api;
    std::unique_ptr<GpuRenderer> r;
    std::string why;
    bool ok(int w, int h, float scale = 1.0f) {
        if (!std::getenv("DISPLAY"))
            return false;
        ctx = gl::GlContext::createOffscreen(w, h, &why);
        if (!ctx || !ctx->makeCurrent() || !ctx->loadApi(api))
            return false;
        r = GpuRenderer::create(api, w, h, scale, true, &why);
        return r != nullptr;
    }
    ~Gpu() {
        r.reset(); // GPU objects die with the context current
    }
};

GpuRenderer::Quality flat() {
    GpuRenderer::Quality q;
    q.bloom = false;
    q.grain = false;
    return q;
}

} // namespace

TEST_CASE("the GPU renderer draws shapes, text and arcs where they belong") {
    Gpu g;
    if (!g.ok(480, 300)) {
        std::printf("gpu_renderer_test: no usable OpenGL 3.3 (%s), skipped\n", g.why.c_str());
        return;
    }
    GpuRenderer& r = *g.r;
    std::printf("renderer: %s, %s\n", g.api.renderer, g.api.version);
    r.setQuality(flat()); // geometry checks without halos

    r.beginFrame(1.0);
    r.clear(rgb(0, 0, 0));
    r.fillRect({10, 10, 60, 40}, rgb(255, 0, 0));
    r.fillRoundRect({80, 10, 60, 40}, 12.0f, rgb(0, 255, 0));
    r.fillCircle({190, 30}, 20.0f, rgb(0, 0, 255));
    r.drawLine({10, 80}, {200, 120}, rgb(255, 255, 255), 3.0f);
    r.drawArc({260, 100}, 30.0f, 2.1f, 7.3f, rgb(255, 200, 0), 6.0f);
    r.drawTextPx("Spux 0123456789", 10, 150, rgb(255, 255, 255), 24.0f, Align::Left, FontWeight::Bold);
    r.drawTextPx("Cutoff 1.2 kHz", 10, 190, rgb(255, 255, 255), 12.0f);
    r.pushClip({300, 200, 40, 40});
    r.fillRect({280, 180, 100, 100}, rgb(255, 0, 255));
    r.popClip();
    r.endFrame();
    std::vector<std::uint32_t> px;
    REQUIRE(r.readPixels(px));
    REQUIRE(px.size() == 480u * 300u);

    CHECK(((at(px, 480, 40, 30) >> 16) & 255u) > 220u);
    CHECK(((at(px, 480, 110, 30) >> 8) & 255u) > 220u);
    CHECK((at(px, 480, 190, 30) & 255u) > 220u);
    CHECK(luma(at(px, 480, 400, 280)) < 5);
    CHECK(((at(px, 480, 81, 11) >> 8) & 255u) < 60u);     // the rounded corner is cut away
    CHECK(((at(px, 480, 320, 220) >> 16) & 255u) > 220u); // the scissor limits the square to 300..340 x 200..240
    CHECK(luma(at(px, 480, 290, 220)) < 5);
    CHECK(luma(at(px, 480, 360, 220)) < 5);
    int lit = 0;
    for (int y = 150; y < 178; ++y)
        for (int x = 10; x < 250; ++x)
            lit += luma(at(px, 480, x, y)) > 128;
    CHECK(lit > 150);
    CHECK(luma(at(px, 480, 260, 100)) < 5); // an arc covers the ring but not the center
}

TEST_CASE("bloom spreads emissive color and the output is deterministic") {
    Gpu g;
    if (!g.ok(256, 128))
        return;
    auto frame = [&](bool bloom) {
        GpuRenderer::Quality q;
        q.bloom = bloom;
        q.grain = false;
        g.r->setQuality(q);
        g.r->beginFrame(0.0);
        g.r->clear(rgb(0, 0, 0));
        g.r->setEmission(4.0f);
        g.r->fillCircle({128, 64}, 6.0f, rgb(60, 160, 255));
        g.r->setEmission(1.0f);
        g.r->endFrame();
        std::vector<std::uint32_t> px;
        g.r->readPixels(px);
        return px;
    };
    const auto a = frame(true), b = frame(true), plain = frame(false);
    CHECK(a == b);
    CHECK(luma(at(a, 256, 128 + 14, 64)) > luma(at(plain, 256, 128 + 14, 64)) + 3); // a halo outside the disc
    CHECK(luma(at(plain, 256, 128 + 14, 64)) < 3);
}

TEST_CASE("the look: bloom strength, vignette and backdrop follow the settings") {
    Gpu g;
    if (!g.ok(256, 128))
        return;
    auto frame = [&](const GpuRenderer::Look& look) {
        g.r->setLook(look);
        GpuRenderer::Quality q;
        q.grain = false;
        g.r->setQuality(q);
        g.r->beginFrame(0.0);
        g.r->drawBackground(rgb(255, 80, 80), rgb(80, 80, 255), 4.0f);
        g.r->endFrame();
        std::vector<std::uint32_t> px;
        g.r->readPixels(px);
        return px;
    };
    GpuRenderer::Look none;
    none.vignette = 0.0f;
    none.backgroundPools = 0.0f;
    none.backgroundDust = 0.0f;
    GpuRenderer::Look full;
    const auto quiet = frame(none), lively = frame(full);
    // no pools: a nearly black field with no vignette, so the corner matches the center
    CHECK(std::abs(luma(at(quiet, 256, 2, 2)) - luma(at(quiet, 256, 128, 64))) <= 8);
    long sumQuiet = 0, sumLively = 0;
    for (std::size_t i = 0; i < quiet.size(); ++i) {
        sumQuiet += luma(quiet[i]);
        sumLively += luma(lively[i]);
    }
    CHECK(sumLively != sumQuiet);
}

TEST_CASE("a framebuffer scale keeps logical coordinates") {
    Gpu g;
    if (!g.ok(100, 60, 2.0f))
        return;
    g.r->setQuality(flat());
    g.r->beginFrame(0.0);
    g.r->clear(rgb(0, 0, 0));
    g.r->fillRect({10, 10, 20, 20}, rgb(255, 255, 255));
    g.r->endFrame();
    std::vector<std::uint32_t> px;
    REQUIRE(g.r->readPixels(px));
    REQUIRE(px.size() == 200u * 120u);
    CHECK(luma(at(px, 200, 30, 30)) > 225);
    CHECK(luma(at(px, 200, 15, 15)) < 5);
    CHECK(luma(at(px, 200, 70, 70)) < 5);
}

TEST_CASE("every painter draws inside its bounds plus margin on the GPU renderer") {
    Gpu g;
    if (!g.ok(280, 240))
        return;
    ModernStyle style;
    g.r->setQuality(flat());
    const Rect b{70, 60, 130, 110};
    KnobState ks;
    ks.theme = &style.theme;
    ks.label = "Knob";
    ks.valueText = "1.0 kHz";
    ks.value = 0.7;
    ks.accent = rgb(255, 186, 84);
    ks.focused = true;
    ks.mod.any = true;
    ks.mod.armed = true;
    ks.mod.base = 0.5;
    ks.mod.shared = 0.8;
    auto render = [&](bool knob) {
        g.r->beginFrame(0.0);
        g.r->clear(rgb(77, 88, 99));
        if (knob)
            style.knob.paint(*g.r, b, ks);
        g.r->endFrame();
        std::vector<std::uint32_t> px;
        g.r->readPixels(px);
        return px;
    };
    const auto base = render(false), withKnob = render(true); // the clear color passes through the tone map
    REQUIRE(base.size() == withKnob.size());
    int outside = 0, inside = 0;
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 280; ++x) {
            const bool changed = at(withKnob, 280, x, y) != at(base, 280, x, y);
            if (b.contains(x, y))
                inside += changed;
            else
                outside += changed;
        }
    CHECK(inside > 500);
    CHECK(outside == 0);
}

TEST_CASE("the showcase renders through the GPU renderer: filled, golden, and the same layout as the software one") {
    Gpu g;
    if (!g.ok(kPageW, kPageH))
        return;
    Host host;
    ShowData d;
    ModernStyle style;
    GpuRenderer::Quality still;
    still.grain = false; // golden frames are free of time-dependent noise
    g.r->setQuality(still);
    std::vector<std::uint32_t> gpu;
    for (int rep = 0; rep < 2; ++rep) { // the second frame sees the first frame's state
        MGui gui(host, style);
        g.r->beginFrame(6.0);
        g.r->drawBackground(kBlue, kViolet, 6.0f);
        gui.begin(*g.r, 6.0, 0.016f, kPageW, kPageH);
        showcasePanels(gui);
        showcaseWidgets(gui, style, d);
        gui.end();
        g.r->endFrame();
    }
    REQUIRE(g.r->readPixels(gpu));
    long lit = 0;
    for (std::uint32_t p : gpu)
        lit += luma(p) > 60;
    CHECK(lit > 15000);
    snapshot("modern_gpu", gpu, kPageW, kPageH);
    checkGolden("modern_gpu", gpu, kPageW, kPageH, 0.97);

    // same layout: the software frame is a plainer rendering of the same page
    SoftRenderer soft = makeSoft(kPageW, kPageH, 2);
    showcaseFrame(soft, host, style, d);
    const double s = ssim(quarter(gpu, kPageW, kPageH), quarter(soft.frame(), kPageW, kPageH), kPageW / 4, kPageH / 4);
    std::printf(
        "GPU against software similarity %.3f; GPU time of the last frame %.2f ms, %d draw calls, %d vertices\n", s,
        static_cast<double>(g.r->gpuMs()), g.r->drawCalls(), g.r->vertexCount());
    CHECK(s > 0.75);
}

#if defined(SPUX_HAS_X11)
namespace {

// The editor's window as the X server shows it: found by size among the root's viewable children, read back.
bool grabWindow(int w, int h, std::vector<std::uint32_t>& out) {
    Display* d = XOpenDisplay(nullptr);
    if (!d)
        return false;
    Window found = 0;
    for (int attempt = 0; attempt < 100 && !found; ++attempt) {
        Window root, parent, *children = nullptr;
        unsigned n = 0;
        XQueryTree(d, DefaultRootWindow(d), &root, &parent, &children, &n);
        for (unsigned i = 0; i < n; ++i) {
            XWindowAttributes a;
            if (XGetWindowAttributes(d, children[i], &a) && a.width == w && a.height == h && a.map_state == IsViewable)
                found = children[i];
        }
        if (children)
            XFree(children);
        if (!found)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    XImage* img =
        found ? XGetImage(d, found, 0, 0, static_cast<unsigned>(w), static_cast<unsigned>(h), AllPlanes, ZPixmap)
              : nullptr;
    if (img) {
        out.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                out[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] =
                    0xFF000000u | static_cast<std::uint32_t>(XGetPixel(img, x, y) & 0xFFFFFFul);
        XDestroyImage(img);
    }
    XCloseDisplay(d);
    return img != nullptr;
}

// Forces frames until `n` more have been drawn: the window's own tick draws too, and a forced frame that finds
// the editor busy is skipped.
void drawFrames(EditorCore& ed, std::uint64_t n, double t0 = 0.0) {
    const std::uint64_t target = ed.framesDrawn() + n;
    for (int i = 0; i < 400 && ed.framesDrawn() < target; ++i) {
        ed.frame(true, t0 + 0.01 * i);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(ed.framesDrawn() >= target);
}

long litPixels(const std::vector<std::uint32_t>& px) {
    long lit = 0;
    for (std::uint32_t p : px)
        lit += luma(p) > 60;
    return lit;
}

} // namespace

TEST_CASE("the editor on a GL window: the GPU draws the showcase, follows the scale, and tears down") {
    if (!std::getenv("DISPLAY"))
        return;
    Host host;
    ModernStyle style;
    ShowRoot root(style);
    EditorOptions o;
    o.width = kPageW;
    o.height = kPageH;
    o.idleHz = 5;
    SurfaceOptions so;
    so.choice = RendererChoice::Gpu; // a software rasterizer is fine for a test
    const ModernBackend backend = modernBackend(so);
    Editor<ModernStyle> ed(host, style, root, o, backend.software, backend.gpu);
    REQUIRE(ed.setParent({platform::WindowApi::X11, 0}));
    if (!ed.usesGpu()) {
        std::printf("gpu_renderer_test: no GL window available, editor on GL skipped\n");
        return;
    }
    std::printf("editor renderer: %s\n", ed.rendererName());
    CHECK(ed.canResize());
    CHECK(ed.width() == kPageW);
    ed.show();
    drawFrames(ed, 3);
    CHECK(ed.lastDirty() == Rect{0, 0, kPageW, kPageH});
    CHECK(ed.degradation() == 0);
    std::vector<std::uint32_t> px;
    REQUIRE(grabWindow(kPageW, kPageH, px));
    CHECK(litPixels(px) > 15000);
    snapshot("modern_editor_gpu", px, kPageW, kPageH);
    // the same page as the offscreen frame: the live frame has grain and a moving backdrop, so not identical
    std::vector<std::uint32_t> ref;
    int rw = 0, rh = 0;
    if (readPpm(std::string(SPUX_TEST_DATA_DIR) + "/golden/modern_gpu.ppm", ref, rw, rh))
        CHECK(ssim(quarter(px, kPageW, kPageH), ref, rw, rh) > 0.9);

    // 1.25x (the virtual screen is 1280 wide): the window grows, the renderer follows in the next frame, the page is
    // the same at a larger size
    REQUIRE(ed.setUserScale(1.25f));
    CHECK(ed.width() == kPageW * 5 / 4);
    drawFrames(ed, 3, 1.0);
    std::vector<std::uint32_t> big;
    REQUIRE(grabWindow(kPageW * 5 / 4, kPageH * 5 / 4, big));
    CHECK(litPixels(big) > 15000 * 3 / 2);
    snapshot("modern_editor_gpu_1_25x", big, kPageW * 5 / 4, kPageH * 5 / 4);

    ed.destroy();
    CHECK_FALSE(ed.usesGpu());
}

TEST_CASE("the editor with the default choice draws either way: on the GPU, or on the software renderer") {
    if (!std::getenv("DISPLAY"))
        return;
    Host host;
    ModernStyle style;
    ShowRoot root(style);
    EditorOptions o;
    o.width = kPageW;
    o.height = kPageH;
    const ModernBackend backend = modernBackend();
    Editor<ModernStyle> ed(host, style, root, o, backend.software, backend.gpu);
    REQUIRE(ed.setParent({platform::WindowApi::X11, 0}));
    ed.show();
    drawFrames(ed, 1);
    std::printf("default choice: %s (%s)\n", ed.usesGpu() ? "GPU" : "software", ed.rendererName());
    if (ed.usesGpu()) {
        const std::string name = ed.rendererName();
        CHECK(name.find("llvmpipe") == std::string::npos); // a software rasterizer is not chosen on its own
        CHECK(name.find("Software") == std::string::npos);
    } else {
        CHECK(std::string(ed.rendererName()) == "software");
        const std::vector<std::uint32_t> px(ed.pixels(), ed.pixels() + kPageW * kPageH);
        CHECK(litPixels(px) > 15000);
        CHECK_FALSE(ed.canResize());
    }
    ed.destroy();
}
#endif
