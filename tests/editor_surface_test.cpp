// The editor shell's GPU path with a fake FrameSurface (no GL): attach and fallback, the call order of a
// frame, window size and scale (user, system, limits, aspect), the mapping of window pixels to logical
// pixels, and teardown order. Needs an X display for the window (xvfb-run); skipped without one.
#include <doctest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "params/TestParams.hpp"
#include "spux/ui/Editor.hpp"
#include "ui/TestRenderer.hpp"
#include "ui/TestStyle.hpp"

#if defined(SPUX_HAS_X11)
#include <X11/Xlib.h>
#endif

using namespace spux::ui;
using namespace testparams;

namespace {

struct Host final : StoreEditorHost<kCount> {
    spux::params::ParamStore<kCount> store{kTable};
    spux::params::EditQueue queue;
    Host() : StoreEditorHost(store, queue) {}
    void requestFlush() override {}
};

struct Log {
    std::vector<std::string> calls;
    float lastScale = 0.0f;
    int resizes = 0, attaches = 0, detaches = 0, begins = 0, ends = 0;
};

class FakeSurface final : public FrameSurface {
public:
    FakeSurface(Log& log, bool attachOk, bool beginOk = true) : log_(log), attachOk_(attachOk), beginOk_(beginOk) {}
    bool attach(spux::platform::NativeWindow&, int w, int h, float scale, std::string* why) override {
        ++log_.attaches;
        log_.lastScale = scale;
        log_.calls.push_back("attach");
        if (!attachOk_) {
            if (why)
                *why = "refused";
            return false;
        }
        renderer_ = std::make_unique<uitest::TestRenderer>(w, h);
        return true;
    }
    bool resize(int, int, float scale) override {
        ++log_.resizes;
        log_.lastScale = scale;
        log_.calls.push_back("resize");
        return true;
    }
    IRenderer* renderer() override { return renderer_.get(); }
    bool beginFrame(double) override {
        log_.calls.push_back("begin");
        ++log_.begins;
        return beginOk_ ? true : (beginOk_ = true, false);
    }
    void endFrame() override {
        log_.calls.push_back("end");
        ++log_.ends;
    }
    void detach() override {
        if (renderer_) {
            ++log_.detaches;
            log_.calls.push_back("detach");
        }
        renderer_.reset();
    }
    const char* name() const override { return "fake"; }
    int degradation() const override { return 2; }

private:
    Log& log_;
    bool attachOk_, beginOk_;
    std::unique_ptr<uitest::TestRenderer> renderer_;
};

// A root view with one full-size interaction: it sees where the pointer is, in logical pixels.
struct Root final : RootView<Gui<uitest::TestStyle>> {
    // written by the window's tick thread, read by the test: atomics
    std::atomic<float> pointerX{-1.0f}, pointerY{-1.0f};
    std::atomic<int> downs{0}, frames{0};
    void frame(Gui<uitest::TestStyle>& g) override {
        ++frames;
        const Interaction i = g.interact(g.id("area"), {0, 0, 400, 300});
        pointerX = i.pointer.x;
        pointerY = i.pointer.y;
        downs += i.pressed;
    }
};

struct Rig {
    Host host;
    uitest::TestStyle style;
    Root root;
    Log log;
    EditorOptions options;
    std::unique_ptr<Editor<uitest::TestStyle>> editor;
    Rig(bool gpu, bool attachOk = true, bool beginOk = true) {
        options.width = 200;
        options.height = 100;
        options.caching = false;
        options.idleHz = 1; // the window's own ticks stay out of the way of the frames the tests force
        SurfaceFactory sf;
        if (gpu)
            sf = [this, attachOk, beginOk] { return std::make_unique<FakeSurface>(log, attachOk, beginOk); };
        editor = std::make_unique<Editor<uitest::TestStyle>>(
            host, style, root, options, [](int w, int h) { return std::make_unique<uitest::TestRenderer>(w, h); }, sf);
    }
    bool open() { return editor->setParent({spux::platform::WindowApi::X11, 0}); }
};

bool haveDisplay() {
#if defined(SPUX_HAS_X11)
    if (!std::getenv("DISPLAY")) {
        std::printf("editor_surface_test: no DISPLAY, skipped\n");
        return false;
    }
    return spux::platform::NativeWindow::supports(spux::platform::WindowApi::X11);
#else
    return false;
#endif
}

#if defined(SPUX_HAS_X11)
// The X window of the editor: the viewable top-level window of this size (the editor is the only one).
Window findWindow(Display* d, int w, int h) {
    for (int attempt = 0; attempt < 50; ++attempt) {
        Window root, parent, *children = nullptr;
        unsigned n = 0;
        XQueryTree(d, DefaultRootWindow(d), &root, &parent, &children, &n);
        Window found = 0;
        for (unsigned i = 0; i < n; ++i) {
            XWindowAttributes a;
            if (XGetWindowAttributes(d, children[i], &a) && a.width == w && a.height == h && a.map_state == IsViewable)
                found = children[i];
        }
        if (children)
            XFree(children);
        if (found)
            return found;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return 0;
}
#endif

} // namespace

TEST_CASE("a surface that attaches: the GPU path, its frame order, scale and limits") {
    if (!haveDisplay())
        return;
    Rig f(true);
    REQUIRE(f.open());
    CHECK(f.editor->usesGpu());
    CHECK(std::string(f.editor->rendererName()) == "fake");
    CHECK(f.editor->degradation() == 2);
    CHECK(f.editor->canResize());
    CHECK(f.log.attaches == 1);
    CHECK(f.editor->scale() == 1.0f);
    CHECK(f.editor->width() == 200);
    CHECK(f.editor->height() == 100);

    f.editor->show();
    REQUIRE(f.editor->frame(true, 0.0));
    REQUIRE(f.log.calls.size() >= 3);
    const std::vector<std::string> first(f.log.calls.begin(), f.log.calls.begin() + 3);
    CHECK(first == std::vector<std::string>{"attach", "begin", "end"}); // begin and end bracket the frame
    CHECK(f.editor->lastDirty() == Rect{0, 0, 200, 100});
    CHECK(f.root.frames == 1);
    // every frame draws everything: nothing is cached on this path
    REQUIRE(f.editor->frame(true, 0.1));
    CHECK(f.root.frames == 2);

    // the host resizes: the scale follows at a fixed aspect, the surface hears of it once, in the next frame
    CHECK(f.editor->setSize(300, 150));
    CHECK(f.editor->scale() == doctest::Approx(1.5f));
    CHECK(f.editor->width() == 300);
    CHECK(f.editor->height() == 150);
    CHECK(f.log.resizes == 0);
    REQUIRE(f.editor->frame(true, 0.2));
    CHECK(f.log.resizes == 1);
    CHECK(f.log.lastScale == doctest::Approx(1.5f));
    REQUIRE(f.editor->frame(true, 0.3));
    CHECK(f.log.resizes == 1);

    // limits: 0.75x to 2x of the logical size
    CHECK(f.editor->setSize(2000, 900));
    CHECK(f.editor->scale() == doctest::Approx(2.0f));
    CHECK(f.editor->setSize(20, 10));
    CHECK(f.editor->scale() == doctest::Approx(0.75f));
    int w = 1000, h = 7;
    f.editor->adjustSize(w, h);
    CHECK(w == 400);
    CHECK(h == 200);
    w = 10;
    f.editor->adjustSize(w, h);
    CHECK(w == 150);
    CHECK(h == 75);
    CHECK_FALSE(f.editor->setSize(0, 10));

    // the plugin's own scale control, and the system's
    CHECK(f.editor->setUserScale(1.0f));
    CHECK(f.editor->userScale() == 1.0f);
    CHECK(f.editor->setScale(2.0));
    CHECK(f.editor->scale() == doctest::Approx(2.0f));
    CHECK(f.editor->width() == 400);
    CHECK_FALSE(f.editor->setScale(0.1));
    CHECK(f.editor->setUserScale(5.0f));
    CHECK(f.editor->userScale() == doctest::Approx(2.0f)); // clamped
    int sw = 100, sh = 50; // with a system scale of 2 the user scale is judged against it
    f.editor->adjustSize(sw, sh);
    CHECK(sw == 300);
    CHECK(sh == 150);

    f.editor->destroy();
    CHECK(f.log.detaches == 1);
    CHECK_FALSE(f.editor->usesGpu());
}

TEST_CASE("a surface that cannot attach: the editor falls back to the CPU renderer") {
    if (!haveDisplay())
        return;
    Rig f(true, false);
    REQUIRE(f.open());
    CHECK_FALSE(f.editor->usesGpu());
    CHECK(std::string(f.editor->rendererName()) == "software");
    CHECK_FALSE(f.editor->canResize());
    CHECK(f.editor->scale() == 1.0f);
    CHECK(f.editor->width() == 200);
    CHECK(f.log.attaches == 1);
    CHECK_FALSE(f.editor->setSize(300, 150)); // the CPU renderers draw at the logical size
    CHECK(f.editor->setSize(200, 100));
    CHECK_FALSE(f.editor->setUserScale(2.0f));
    CHECK_FALSE(f.editor->setScale(2.0));
    int w = 321, h = 77;
    f.editor->adjustSize(w, h);
    CHECK(w == 200);
    CHECK(h == 100);
    CHECK(f.editor->degradation() == 0);
    f.editor->show();
    REQUIRE(f.editor->frame(true, 0.0));
    CHECK(f.log.begins == 0);
    CHECK(f.editor->pixels()[0] == (f.style.theme.colors.background | 0xFF000000u)); // the CPU frame is there to read
    f.editor->destroy();
}

TEST_CASE("without a surface factory nothing changes") {
    if (!haveDisplay())
        return;
    Rig f(false);
    REQUIRE(f.open());
    CHECK_FALSE(f.editor->usesGpu());
    CHECK(f.log.attaches == 0);
    f.editor->show();
    CHECK(f.editor->frame(true, 0.0));
    f.editor->destroy();
}

TEST_CASE("a frame the surface cannot begin is skipped and retried") {
    if (!haveDisplay())
        return;
    Rig f(true, true, false);
    REQUIRE(f.open());
    f.editor->show();
    CHECK_FALSE(f.editor->frame(true, 0.0)); // beginFrame refused once
    CHECK(f.log.ends == 0);
    CHECK(f.root.frames == 0);
    CHECK(f.editor->frame(false, 0.1)); // the retry needs no new input
    CHECK(f.log.ends == 1);
    f.editor->destroy();
}

#if defined(SPUX_HAS_X11)
TEST_CASE("window pixels are logical pixels times the scale, and the window follows the scale") {
    if (!haveDisplay())
        return;
    Rig f(true);
    REQUIRE(f.open());
    f.editor->show();
    REQUIRE(f.editor->setUserScale(1.5f)); // 300 x 150 pixels
    REQUIRE(f.editor->frame(true, 0.0));
    Display* d = XOpenDisplay(nullptr);
    REQUIRE(d);
    const Window win = findWindow(d, 300, 150); // the window was resized to follow the scale
    REQUIRE(win != 0);

    XEvent e = {};
    e.xbutton.type = ButtonPress;
    e.xbutton.window = win;
    e.xbutton.x = 150;
    e.xbutton.y = 75;
    e.xbutton.button = Button1;
    XSendEvent(d, win, False, ButtonPressMask, &e);
    XFlush(d);
    for (int i = 0; i < 100 && f.root.downs == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        f.editor->frame(true, 0.1 + 0.01 * i);
    }
    CHECK(f.root.downs == 1);
    CHECK(f.root.pointerX.load() == doctest::Approx(100.0f)); // 150 px at 1.5x
    CHECK(f.root.pointerY.load() == doctest::Approx(50.0f));
    XCloseDisplay(d);
    f.editor->destroy();
}
#endif
