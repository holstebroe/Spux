// X11 child window. Own Display connection per window,
// so several plugin instances in one host do not interfere; every Xlib call is made under a mutex.
// TODO: dropped files (WindowOptions::acceptFiles) need the XDND protocol: XdndAware on the window, and
// answers to XdndEnter, XdndPosition and XdndDrop with a selection request for text/uri-list.
#if defined(SPUX_HAS_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <memory>
#include <poll.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "GlxVisual.hpp"
#include "spux/platform/NativeWindow.hpp"

namespace spux::inline v1::platform {
namespace {

class X11Window final : public NativeWindow {
public:
    ~X11Window() override { close(); }

    bool open(const ParentWindow& parent, int width, int height, WindowEvents* events,
              const WindowOptions& options) override {
        if (open_ || parent.api != WindowApi::X11 || width <= 0 || height <= 0)
            return false;
        // The server can refuse a connection for a moment, most often right after another window of this
        // process closed its own: try again briefly before giving up. Without a display there is nothing
        // to wait for.
        Display* d = XOpenDisplay(nullptr);
        for (int attempt = 0; !d && attempt < 20 && std::getenv("DISPLAY"); ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            d = XOpenDisplay(nullptr);
        }
        if (!d)
            return false;
        const int screen = DefaultScreen(d);
        const Window p = parent.handle ? static_cast<Window>(parent.handle) : RootWindow(d, screen);
        Window w;
        XVisualInfo vi;
        if (options.openGl) {
            // A GL window needs the visual and colormap GLX picked: a child may not inherit the parent's.
            if (!glxChooseWindowVisual(d, screen, vi)) {
                XCloseDisplay(d);
                return false;
            }
            XSetWindowAttributes attr = {};
            attr.colormap = XCreateColormap(d, p, vi.visual, AllocNone);
            attr.border_pixel = 0;
            attr.background_pixel = 0;
            w = XCreateWindow(d, p, 0, 0, static_cast<unsigned>(width), static_cast<unsigned>(height), 0, vi.depth,
                              InputOutput, vi.visual, CWColormap | CWBorderPixel | CWBackPixel, &attr);
            colormap_ = attr.colormap;
        } else {
            w = XCreateSimpleWindow(d, p, 0, 0, static_cast<unsigned>(width), static_cast<unsigned>(height), 0,
                                    BlackPixel(d, screen), BlackPixel(d, screen));
        }
        XSelectInput(d, w,
                     ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | LeaveWindowMask |
                         EnterWindowMask | (options.keyboard ? KeyPressMask : 0));
        XMapWindow(d, w);
        XFlush(d);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            display_ = d;
            window_ = w;
            events_ = events;
            visible_ = true;
            open_ = true;
            gl_ = options.openGl;
            keyboard_ = options.keyboard;
        }
        running_ = true;
        thread_ = std::thread([this] { loop(); });
        return true;
    }

    void close() override {
        running_ = false;
        if (thread_.joinable())
            thread_.join();
        std::lock_guard<std::mutex> lock(mutex_);
        if (!open_)
            return;
        XDestroyWindow(display_, window_);
        if (colormap_)
            XFreeColormap(display_, colormap_);
        colormap_ = 0;
        XCloseDisplay(display_);
        display_ = nullptr;
        open_ = false;
    }

    void setSize(int width, int height) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!open_ || width <= 0 || height <= 0)
            return;
        XResizeWindow(display_, window_, static_cast<unsigned>(width), static_cast<unsigned>(height));
        XFlush(display_);
    }

    void setVisible(bool visible) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!open_)
            return;
        visible_ = visible;
        if (visible)
            XMapWindow(display_, window_);
        else
            XUnmapWindow(display_, window_);
        XFlush(display_);
    }

    void setTickRate(int hz) override { tickMs_ = 1000 / std::clamp(hz, 1, 120); }

    void present(const std::uint32_t* argb, int width, int height, const DirtyRect& dirty) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!open_ || gl_ || !argb || width <= 0 || height <= 0)
            return;
        // Clip to the image: an out-of-range XPutImage raises an X protocol error, whose default
        // handler exits the whole host process.
        const int x0 = std::clamp(dirty.x, 0, width), y0 = std::clamp(dirty.y, 0, height);
        const int x1 = std::clamp(dirty.x + dirty.w, 0, width), y1 = std::clamp(dirty.y + dirty.h, 0, height);
        if (x1 <= x0 || y1 <= y0)
            return;
        const int screen = DefaultScreen(display_);
        XImage* image = XCreateImage(display_, DefaultVisual(display_, screen), 24, ZPixmap, 0,
                                     reinterpret_cast<char*>(const_cast<std::uint32_t*>(argb)),
                                     static_cast<unsigned>(width), static_cast<unsigned>(height), 32, 0);
        if (!image)
            return;
        XPutImage(display_, window_, DefaultGC(display_, screen), image, x0, y0, x0, y0, static_cast<unsigned>(x1 - x0),
                  static_cast<unsigned>(y1 - y0));
        image->data = nullptr; // the caller owns the pixels
        XDestroyImage(image);
        XFlush(display_);
    }

    bool isOpen() const override { return open_; }
    ParentWindow nativeHandle() const override { return {WindowApi::X11, static_cast<std::uintptr_t>(window_)}; }

private:
    struct Ev {
        int type;
        int x, y;
        unsigned state;
        unsigned button;
        KeyEvent key;
    };

    void loop() {
        try {
            auto lastTick = std::chrono::steady_clock::now();
            int fd;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                fd = ConnectionNumber(display_);
            }
            std::vector<Ev> batch;
            while (running_) {
                pollfd pfd{fd, POLLIN, 0};
                const auto sinceTick =
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastTick)
                        .count();
                poll(&pfd, 1, static_cast<int>(std::clamp<long long>(tickMs_.load() - sinceTick, 1, 16)));
                batch.clear();
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    while (XPending(display_) > 0) {
                        XEvent e;
                        XNextEvent(display_, &e);
                        switch (e.type) {
                        case Expose:
                            if (e.xexpose.count == 0)
                                batch.push_back({Expose, 0, 0, 0, 0, {}});
                            break;
                        case ButtonPress:
                        case ButtonRelease:
                            batch.push_back({e.type, e.xbutton.x, e.xbutton.y, e.xbutton.state, e.xbutton.button, {}});
                            // a click gives the editor the keyboard (a host window may not pass it on otherwise)
                            if (keyboard_ && e.type == ButtonPress && e.xbutton.button == Button1)
                                XSetInputFocus(display_, window_, RevertToParent, CurrentTime);
                            break;
                        case MotionNotify:
                            batch.push_back({MotionNotify, e.xmotion.x, e.xmotion.y, e.xmotion.state, 0, {}});
                            break;
                        case LeaveNotify:
                            batch.push_back({LeaveNotify, 0, 0, 0, 0, {}});
                            break;
                        case KeyPress: {
                            Ev ev{KeyPress, 0, 0, e.xkey.state, 0, {}};
                            if (translateKey(e.xkey, ev.key))
                                batch.push_back(ev);
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
                WindowEvents* ev = events_; // callbacks run without the lock so they may present()
                for (const Ev& e : batch) {
                    if (!ev)
                        break;
                    const bool shift = (e.state & ShiftMask) != 0;
                    if (e.type == ButtonPress || e.type == MotionNotify)
                        ev->onCtrl((e.state & ControlMask) != 0);
                    if (e.type == Expose)
                        ev->onExpose();
                    else if (e.type == LeaveNotify)
                        ev->onMouseLeave();
                    else if (e.type == KeyPress)
                        ev->onKey(e.key);
                    else if (e.type == ButtonPress && (e.button == Button4 || e.button == Button5))
                        ev->onMouseWheel(e.x, e.y, e.button == Button4 ? 1.0f : -1.0f, shift);
                    else if (e.type == ButtonPress && e.button == Button1)
                        ev->onMouseDown(e.x, e.y, MouseButton::Left, shift);
                    else if (e.type == ButtonPress && e.button == Button3)
                        ev->onMouseDown(e.x, e.y, MouseButton::Right, shift);
                    else if (e.type == ButtonRelease && e.button == Button1)
                        ev->onMouseUp(e.x, e.y, MouseButton::Left);
                    else if (e.type == ButtonRelease && e.button == Button3)
                        ev->onMouseUp(e.x, e.y, MouseButton::Right);
                    else if (e.type == MotionNotify)
                        ev->onMouseMove(e.x, e.y, (e.state & Button1Mask) != 0, shift);
                }
                const auto now = std::chrono::steady_clock::now();
                if (visible_ && ev && now - lastTick >= std::chrono::milliseconds(tickMs_.load())) {
                    lastTick = now;
                    ev->onTick();
                }
            }
        } catch (...) {
            // An exception escaping a thread function would std::terminate the host.
        }
    }

    static bool translateKey(XKeyEvent& xk, KeyEvent& out) {
        char buf[8] = {};
        KeySym sym = NoSymbol;
        const int n = XLookupString(&xk, buf, sizeof(buf) - 1, &sym, nullptr);
        out.shift = (xk.state & ShiftMask) != 0;
        out.ctrl = (xk.state & ControlMask) != 0;
        switch (sym) {
        case XK_Return:
        case XK_KP_Enter:
            out.key = KeyEvent::Key::Enter;
            return true;
        case XK_Escape:
            out.key = KeyEvent::Key::Escape;
            return true;
        case XK_BackSpace:
            out.key = KeyEvent::Key::Backspace;
            return true;
        case XK_Delete:
            out.key = KeyEvent::Key::Delete;
            return true;
        case XK_Left:
            out.key = KeyEvent::Key::Left;
            return true;
        case XK_Right:
            out.key = KeyEvent::Key::Right;
            return true;
        case XK_Up:
            out.key = KeyEvent::Key::Up;
            return true;
        case XK_Down:
            out.key = KeyEvent::Key::Down;
            return true;
        case XK_Home:
            out.key = KeyEvent::Key::Home;
            return true;
        case XK_End:
            out.key = KeyEvent::Key::End;
            return true;
        case XK_Tab:
            out.key = KeyEvent::Key::Tab;
            return true;
        default:
            break;
        }
        // The text is UTF-8: the code point comes from the keysym (Latin-1 keysyms are their code point,
        // Unicode keysyms carry it in the low bits), not from XLookupString's Latin-1 bytes. Other legacy
        // keysyms are not typed.
        if (n <= 0)
            return false;
        if (out.ctrl && (xk.state & Mod1Mask || sym < 'a' || sym > 'z'))
            return false; // only ctrl+letter is a shortcut (ctrl+alt is a layout's typing)
        std::uint32_t cp = 0;
        if (sym >= 0x20 && sym <= 0xff)
            cp = static_cast<std::uint32_t>(sym);
        else if (sym >= 0x01000100 && sym <= 0x0110ffff)
            cp = static_cast<std::uint32_t>(sym - 0x01000000);
        if (cp < 0x20 || cp == 0x7f || (cp >= 0x80 && cp < 0xa0) || (cp >= 0xd800 && cp <= 0xdfff))
            return false;
        out.key = KeyEvent::Key::Char;
        char* t = out.text;
        if (cp < 0x80)
            *t++ = static_cast<char>(cp);
        else if (cp < 0x800) {
            *t++ = static_cast<char>(0xc0 | (cp >> 6));
            *t++ = static_cast<char>(0x80 | (cp & 0x3f));
        } else if (cp < 0x10000) {
            *t++ = static_cast<char>(0xe0 | (cp >> 12));
            *t++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
            *t++ = static_cast<char>(0x80 | (cp & 0x3f));
        } else {
            *t++ = static_cast<char>(0xf0 | (cp >> 18));
            *t++ = static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
            *t++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
            *t++ = static_cast<char>(0x80 | (cp & 0x3f));
        }
        *t = 0;
        return true;
    }

    std::mutex mutex_;
    Display* display_ = nullptr;
    Window window_ = 0;
    Colormap colormap_ = 0;
    bool gl_ = false;
    bool keyboard_ = true;
    WindowEvents* events_ = nullptr;
    std::atomic<int> tickMs_{33};
    std::atomic<bool> running_{false};
    std::atomic<bool> visible_{false};
    bool open_ = false;
    std::thread thread_;
};

} // namespace

bool NativeWindow::supports(WindowApi api) { return api == WindowApi::X11; }
std::unique_ptr<NativeWindow> NativeWindow::create() { return std::make_unique<X11Window>(); }

} // namespace spux::inline v1::platform
#endif
