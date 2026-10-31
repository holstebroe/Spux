// Native window glue (ported from Plumeflow's gfx_window_test, without its editor cases): opens an X11
// child window of the root window, presents a frame, reads it back, and checks that synthetic mouse
// events and ticks reach the callbacks. Skipped without a display (run it under xvfb-run); it never
// fails just because no X server is available.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

#include "UriList.hpp"
#include "doctest.h"
#include "spux/platform/NativeWindow.hpp"

#if defined(SPUX_HAS_X11)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

using namespace spux::platform;

namespace {
struct Sink : WindowEvents {
    std::atomic<int> downs{0}, ups{0}, moves{0}, ticks{0}, exposes{0}, lastX{-1}, lastY{-1};
    std::atomic<bool> lastShift{false};
    void onMouseDown(int x, int y, MouseButton b, bool shift) override {
        if (b == MouseButton::Left) {
            ++downs;
            lastX = x;
            lastY = y;
            lastShift = shift;
        }
    }
    void onMouseUp(int, int, MouseButton b) override {
        if (b == MouseButton::Left)
            ++ups;
    }
    void onMouseMove(int, int, bool, bool) override { ++moves; }
    void onTick() override { ++ticks; }
    void onExpose() override { ++exposes; }
    void onFilesDropped(const std::vector<std::filesystem::path>& f) override {
        std::lock_guard<std::mutex> lock(mutex);
        files = f;
    }
    std::mutex mutex;
    std::vector<std::filesystem::path> files;
};
// Under heavy parallel load the X server can refuse a connection for a moment: retry a few times.
[[maybe_unused]] bool openRetry(NativeWindow& w, int width, int height, WindowEvents* ev) {
    for (int i = 0; i < 20; ++i) {
        if (w.open({WindowApi::X11, 0}, width, height, ev))
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}
} // namespace

#if defined(SPUX_HAS_X11)
TEST_CASE("X11 window: present, read back, input and ticks") {
    if (!std::getenv("DISPLAY")) {
        std::printf("platform_window_test: no DISPLAY, skipped\n");
        return;
    }
    REQUIRE(NativeWindow::supports(WindowApi::X11));
    REQUIRE_FALSE(NativeWindow::supports(WindowApi::Win32));

    constexpr int W = 96, H = 64;
    Sink sink;
    auto win = NativeWindow::create();
    REQUIRE(win);
    REQUIRE(openRetry(*win, W, H, &sink));
    CHECK(win->isOpen());

    std::vector<std::uint32_t> frame(W * H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            frame[static_cast<std::size_t>(y * W + x)] = 0xFF000000u | (static_cast<std::uint32_t>(x * 2) << 16) |
                                                         (static_cast<std::uint32_t>(y * 3) << 8) | 0x40u;

    // wait for the window to be mapped, then present the whole frame and a dirty sub-rectangle
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    win->present(frame.data(), W, H, {0, 0, W, H});
    // a partial update with out-of-range rectangles must not crash or raise an X error
    win->present(frame.data(), W, H, {-10, -10, 500, 500});
    win->present(frame.data(), W, H, {W + 5, H + 5, 10, 10});

    // read the window back through a second connection
    Display* d = XOpenDisplay(nullptr);
    REQUIRE(d);
    Window root, parent, *children = nullptr;
    unsigned n = 0;
    XQueryTree(d, DefaultRootWindow(d), &root, &parent, &children, &n);
    Window found = 0;
    for (unsigned i = 0; i < n; ++i) {
        XWindowAttributes a;
        if (XGetWindowAttributes(d, children[i], &a) && a.width == W && a.height == H && a.map_state == IsViewable)
            found = children[i];
    }
    if (children)
        XFree(children);
    REQUIRE(found != 0);
    XImage* img = XGetImage(d, found, 0, 0, W, H, AllPlanes, ZPixmap);
    REQUIRE(img);
    int bad = 0;
    for (int y = 0; y < H; y += 7)
        for (int x = 0; x < W; x += 5) {
            const unsigned long p = XGetPixel(img, x, y) & 0xFFFFFFul;
            if (p != (frame[static_cast<std::size_t>(y * W + x)] & 0xFFFFFFu))
                ++bad;
        }
    CHECK(bad == 0);
    XDestroyImage(img);

    // synthetic input
    XEvent e = {};
    e.xbutton.type = ButtonPress;
    e.xbutton.window = found;
    e.xbutton.x = 17;
    e.xbutton.y = 23;
    e.xbutton.button = Button1;
    e.xbutton.state = ShiftMask;
    XSendEvent(d, found, False, ButtonPressMask, &e);
    e.xbutton.type = ButtonRelease;
    XSendEvent(d, found, False, ButtonReleaseMask, &e);
    XMotionEvent m = {};
    m.type = MotionNotify;
    m.window = found;
    m.x = 5;
    m.y = 6;
    XSendEvent(d, found, False, PointerMotionMask, reinterpret_cast<XEvent*>(&m));
    XFlush(d);

    for (int i = 0; i < 100 && (sink.downs == 0 || sink.ticks < 2); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK(sink.downs == 1);
    CHECK(sink.lastX == 17);
    CHECK(sink.lastY == 23);
    CHECK(sink.lastShift);
    CHECK(sink.ups == 1);
    CHECK(sink.moves >= 1);
    CHECK(sink.ticks >= 2);

    // a hidden window stops ticking
    win->setVisible(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const int t0 = sink.ticks;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    CHECK(sink.ticks == t0);

    XCloseDisplay(d);
    win->close();
    CHECK_FALSE(win->isOpen());
}

TEST_CASE("two windows at once do not interfere") {
    if (!std::getenv("DISPLAY"))
        return;
    Sink a, b;
    auto w1 = NativeWindow::create(), w2 = NativeWindow::create();
    REQUIRE(openRetry(*w1, 40, 30, &a));
    REQUIRE(openRetry(*w2, 50, 20, &b));
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    w1->close();
    const int tb = b.ticks;
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    CHECK(b.ticks > tb); // the second window keeps running after the first closed
    w2->close();
}
TEST_CASE("uri lists become local paths") {
    const auto paths = uriListPaths("# a comment\r\nfile:///music/a%20b.syx\r\nhttp://x/y\r\nfile://host/c.mid\r\n");
    REQUIRE(paths.size() == 2);
    CHECK(paths[0] == "/music/a b.syx");
    CHECK(paths[1] == "/c.mid");
    CHECK(uriListPaths("").empty());
}

// A second X client plays the drag source of the XDND protocol: it answers the target's XdndPosition with
// a status, owns the XdndSelection and serves text/uri-list when the target asks for it after XdndDrop.
TEST_CASE("X11 window: a file drop (XDND) reaches onFilesDropped") {
    if (!std::getenv("DISPLAY")) {
        std::printf("platform_window_test: no DISPLAY, skipped\n");
        return;
    }
    Sink sink;
    auto win = NativeWindow::create();
    WindowOptions options;
    options.acceptFiles = true;
    REQUIRE(win->open({WindowApi::X11, 0}, 200, 100, &sink, options));
    const Window target = static_cast<Window>(win->nativeHandle().handle);

    Display* d = XOpenDisplay(nullptr);
    REQUIRE(d);
    const Atom aware = XInternAtom(d, "XdndAware", False), position = XInternAtom(d, "XdndPosition", False),
               drop = XInternAtom(d, "XdndDrop", False), status = XInternAtom(d, "XdndStatus", False),
               finished = XInternAtom(d, "XdndFinished", False), selection = XInternAtom(d, "XdndSelection", False),
               uriList = XInternAtom(d, "text/uri-list", False);
    // the target announces itself as a drop target
    Atom type;
    int format;
    unsigned long count = 0, after = 0;
    unsigned char* data = nullptr;
    REQUIRE(XGetWindowProperty(d, target, aware, 0, 1, False, XA_ATOM, &type, &format, &count, &after, &data) ==
            Success);
    REQUIRE(data != nullptr);
    CHECK(*reinterpret_cast<Atom*>(data) == 5);
    XFree(data);

    const Window source = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, 10, 10, 0, 0, 0);
    XSetSelectionOwner(d, selection, source, CurrentTime);
    auto send = [&](Atom message, long l1, long l2) {
        XEvent m{};
        m.xclient.type = ClientMessage;
        m.xclient.window = target;
        m.xclient.message_type = message;
        m.xclient.format = 32;
        m.xclient.data.l[0] = static_cast<long>(source);
        m.xclient.data.l[1] = l1;
        m.xclient.data.l[2] = l2;
        XSendEvent(d, target, False, NoEventMask, &m);
        XFlush(d);
    };
    send(position, 0, (10L << 16) | 10L);
    send(drop, 0, CurrentTime);

    bool gotStatus = false, gotFinished = false;
    long finishedAccepted = -1;
    const std::string list = "file:///tmp/a%20b.syx\r\nfile:///tmp/c.mid\r\n";
    for (int i = 0; i < 200 && !gotFinished; ++i) {
        while (XPending(d) > 0) {
            XEvent e;
            XNextEvent(d, &e);
            if (e.type == ClientMessage && static_cast<Atom>(e.xclient.message_type) == status)
                gotStatus = e.xclient.data.l[1] & 1;
            else if (e.type == ClientMessage && static_cast<Atom>(e.xclient.message_type) == finished) {
                gotFinished = true;
                finishedAccepted = e.xclient.data.l[1];
            } else if (e.type == SelectionRequest) {
                const XSelectionRequestEvent& r = e.xselectionrequest;
                CHECK(r.target == uriList); // the target asks for text/uri-list
                XChangeProperty(d, r.requestor, r.property, r.target, 8, PropModeReplace,
                                reinterpret_cast<const unsigned char*>(list.data()), static_cast<int>(list.size()));
                XEvent n{};
                n.xselection.type = SelectionNotify;
                n.xselection.requestor = r.requestor;
                n.xselection.selection = r.selection;
                n.xselection.target = r.target;
                n.xselection.property = r.property;
                n.xselection.time = r.time;
                XSendEvent(d, r.requestor, False, NoEventMask, &n);
                XFlush(d);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(gotStatus);
    CHECK(gotFinished);
    CHECK(finishedAccepted == 1);
    for (int i = 0; i < 100; ++i) {
        {
            std::lock_guard<std::mutex> lock(sink.mutex);
            if (!sink.files.empty())
                break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::lock_guard<std::mutex> lock(sink.mutex);
    REQUIRE(sink.files.size() == 2);
    CHECK(sink.files[0] == "/tmp/a b.syx");
    CHECK(sink.files[1] == "/tmp/c.mid");
    XDestroyWindow(d, source);
    XCloseDisplay(d);
    win->close();
}
#else
TEST_CASE("native window glue is only tested on Linux here") { CHECK(true); }
#endif
