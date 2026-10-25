// Native child window for the editor (X11 and Win32). It owns a window inside the host's parent window,
// turns native input into WindowEvents and blits ARGB pixels to it. It knows nothing about the UI:
// a software-rendered frame is presented as pixels, a GPU frame will present through its own
// context. One instance per editor; nothing is shared between instances.
#pragma once
#include <cstdint>
#include <memory>

namespace spux::inline v1::platform {

enum class WindowApi { X11, Win32, Cocoa };

struct ParentWindow {
    WindowApi api;
    std::uintptr_t handle; // X11 Window, HWND or NSView*; 0 on X11 means the root window (tests)
};

struct DirtyRect {
    int x = 0, y = 0, w = 0, h = 0;
};

enum class MouseButton { Left, Right };

// A key press as the editor needs it: editing keys, navigation and typed text (UTF-8, already composed). A
// shortcut (ctrl held, no alt) is a Char with `ctrl` set and the lower-case letter as its text; text entry ignores it.
struct KeyEvent {
    enum class Key { Unknown, Char, Enter, Escape, Backspace, Delete, Left, Right, Up, Down, Home, End, Tab };
    Key key = Key::Unknown;
    char text[8] = {}; // Key::Char only
    bool shift = false, ctrl = false;
};

// What the window is going to be used for. A GPU window needs a pixel format chosen when it is created.
struct WindowOptions {
    bool openGl = false;
    // The window takes the keyboard focus on a click and reports keys through onKey. A window that has
    // no use for keys sets this to false, so the host keeps its shortcuts (transport, undo).
    bool keyboard = true;
};

// Callbacks run on the window's event thread (X11) or the host UI thread (Win32); they may call
// NativeWindow::present() and must not block.
class WindowEvents {
public:
    virtual ~WindowEvents() = default;
    virtual void onMouseDown(int /*x*/, int /*y*/, MouseButton, bool /*shift*/) {}
    virtual void onMouseUp(int /*x*/, int /*y*/, MouseButton) {}
    virtual void onMouseMove(int /*x*/, int /*y*/, bool /*leftDown*/, bool /*shift*/) {}
    virtual void onMouseWheel(int /*x*/, int /*y*/, float /*steps*/, bool /*shift*/) {} // positive: away from the user
    virtual void onMouseLeave() {}
    virtual void onKey(const KeyEvent&) {}
    virtual void onTick() {}   // at the tick rate while the window is visible
    virtual void onExpose() {} // the window contents were lost: present the whole frame again
};

class NativeWindow {
public:
    virtual ~NativeWindow() = default;

    // The window API this build can host (CLAP window APIs map onto it by the plugin).
    static bool supports(WindowApi api);
    // nullptr when the platform has no implementation.
    static std::unique_ptr<NativeWindow> create();

    virtual bool open(const ParentWindow& parent, int width, int height, WindowEvents* events,
                      const WindowOptions& options = {}) = 0;
    // The window itself (an X11 Window or an HWND), for attaching a GL context. Valid while open.
    virtual ParentWindow nativeHandle() const = 0;
    // Pixels per logical pixel the system suggests for this window (1 when unknown).
    virtual float contextScale() const { return 1.0f; }
    virtual void close() = 0;
    virtual void setSize(int width, int height) = 0;
    virtual void setVisible(bool visible) = 0;
    // How often onTick runs while the window is visible (1..120 Hz, 30 by default).
    virtual void setTickRate(int hz) = 0;
    // Copies the dirty rectangle of an ARGB frame (width x height, row-major) to the window.
    // Safe to call from any thread.
    virtual void present(const std::uint32_t* argb, int width, int height, const DirtyRect& dirty) = 0;
    virtual bool isOpen() const = 0;
};

} // namespace spux::inline v1::platform
