// Win32 child window. The window lives on the host's
// UI thread and uses its message loop, so no thread of our own. NOT YET COMPILED OR RUN: this
// machine has no Windows toolchain; the first CI run on windows-latest is the check.
//
// The window class is registered by the module (plugin DLL) that contains this code, under a name made
// from that module's handle: the host process and other plugins, possibly with another spux version,
// never share or clash with it.
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <memory>
#include <windows.h>

#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <mutex>
#include <string>

#include "spux/platform/NativeWindow.hpp"

namespace spux::inline v1::platform {
namespace {

constexpr UINT_PTR kTimerId = 1;

class Win32Window;

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

class Win32Window final : public NativeWindow {
public:
    ~Win32Window() override { close(); }

    bool open(const ParentWindow& parent, int width, int height, WindowEvents* events,
              const WindowOptions& options) override {
        if (hwnd_ || parent.api != WindowApi::Win32 || width <= 0 || height <= 0)
            return false;
        // The module that holds this code (the plugin DLL), not the host executable.
        HMODULE inst = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR>(&wndProc), &inst))
            return false;
        static wchar_t className[48] = {};
        static std::once_flag once;
        static bool registered = false;
        std::call_once(once, [inst] {
            swprintf(className, sizeof(className) / sizeof(wchar_t), L"SpuxWindow%llx",
                     static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(inst)));
            WNDCLASSW wc = {};
            wc.lpfnWndProc = wndProc;
            wc.hInstance = inst;
            wc.lpszClassName = className;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.style = CS_OWNDC | CS_DBLCLKS;
            registered = RegisterClassW(&wc) != 0;
        });
        if (!registered)
            return false;
        events_ = events;
        gl_ = options.openGl;
        keyboard_ = options.keyboard;
        hwnd_ = CreateWindowExW(options.acceptFiles ? WS_EX_ACCEPTFILES : 0, className, L"spux", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0,
                                width, height, reinterpret_cast<HWND>(parent.handle), nullptr, inst, this);
        return hwnd_ != nullptr;
    }

    void close() override {
        if (!hwnd_)
            return;
        HWND h = hwnd_;
        hwnd_ = nullptr;
        DestroyWindow(h);
    }

    void setSize(int width, int height) override {
        if (hwnd_ && width > 0 && height > 0)
            SetWindowPos(hwnd_, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
    }

    void setVisible(bool visible) override {
        visible_ = visible;
        if (hwnd_)
            ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);
    }

    void setTickRate(int hz) override {
        if (hwnd_)
            SetTimer(hwnd_, kTimerId, static_cast<UINT>(1000 / std::clamp(hz, 1, 120)),
                     nullptr); // replaces the running timer
    }

    void present(const std::uint32_t* argb, int width, int height, const DirtyRect& dirty) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!hwnd_ || gl_ || !argb || width <= 0 || height <= 0)
            return;
        const int x0 = std::clamp(dirty.x, 0, width), y0 = std::clamp(dirty.y, 0, height);
        const int x1 = std::clamp(dirty.x + dirty.w, 0, width), y1 = std::clamp(dirty.y + dirty.h, 0, height);
        if (x1 <= x0 || y1 <= y0)
            return;
        HDC hdc = GetDC(hwnd_);
        if (!hdc)
            return;
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        // The whole frame: the meaning of the source y of a partial blit from a top-down
        // DIB is easy to get wrong, and a full-frame blit of a panel this size is cheap.
        SetDIBitsToDevice(hdc, 0, 0, static_cast<DWORD>(width), static_cast<DWORD>(height), 0, 0, 0,
                          static_cast<UINT>(height), argb, &bmi, DIB_RGB_COLORS);
        ReleaseDC(hwnd_, hdc);
    }

    bool isOpen() const override { return hwnd_ != nullptr; }
    ParentWindow nativeHandle() const override { return {WindowApi::Win32, reinterpret_cast<std::uintptr_t>(hwnd_)}; }
    float contextScale() const override {
        using GetDpiFn = UINT(WINAPI*)(HWND);
        static const auto fn =
            reinterpret_cast<GetDpiFn>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
        return hwnd_ && fn ? static_cast<float>(fn(hwnd_)) / 96.0f : 1.0f;
    }

    WindowEvents* events() const { return events_; }
    bool keyboard() const { return keyboard_; }
    bool visible() const { return visible_; }
    bool trackedLeave = false;

private:
    std::mutex mutex_;
    HWND hwnd_ = nullptr;
    WindowEvents* events_ = nullptr;
    std::atomic<bool> visible_{true};
    bool gl_ = false;
    bool keyboard_ = true;
};

LRESULT handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    WindowEvents* ev = self ? self->events() : nullptr;
    const auto x = static_cast<int>(static_cast<short>(LOWORD(lp)));
    const auto y = static_cast<int>(static_cast<short>(HIWORD(lp)));
    switch (msg) {
    case WM_CREATE:
        self = static_cast<Win32Window*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        SetTimer(hwnd, kTimerId, 33, nullptr); // ~30 fps
        return 0;
    case WM_TIMER:
        if (ev && self->visible())
            ev->onTick();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        if (ev)
            ev->onExpose();
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetCapture(hwnd);
        if (!self || self->keyboard())
            SetFocus(hwnd);
        if (ev)
            ev->onMouseDown(x, y, MouseButton::Left, (wp & MK_SHIFT) != 0);
        return 0;
    case WM_RBUTTONDOWN:
        if (ev)
            ev->onMouseDown(x, y, MouseButton::Right, (wp & MK_SHIFT) != 0);
        return 0;
    case WM_MOUSEWHEEL: {
        POINT pt = {static_cast<LONG>(static_cast<short>(LOWORD(lp))),
                    static_cast<LONG>(static_cast<short>(HIWORD(lp)))};
        ScreenToClient(hwnd, &pt); // wheel coordinates are in screen space
        if (ev)
            ev->onMouseWheel(pt.x, pt.y,
                             static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / static_cast<float>(WHEEL_DELTA),
                             (GET_KEYSTATE_WPARAM(wp) & MK_SHIFT) != 0);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (self)
            self->trackedLeave = false;
        if (ev)
            ev->onMouseLeave();
        return 0;
    case WM_GETDLGCODE:
        // A host that shows plugins in a dialog (IsDialogMessage) would otherwise keep keys like ctrl+V for itself.
        if (self && self->keyboard())
            return DLGC_WANTALLKEYS;
        break;
    case WM_KEYDOWN: {
        KeyEvent k;
        k.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        k.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        switch (wp) {
        case VK_RETURN:
            k.key = KeyEvent::Key::Enter;
            break;
        case VK_ESCAPE:
            k.key = KeyEvent::Key::Escape;
            break;
        case VK_BACK:
            k.key = KeyEvent::Key::Backspace;
            break;
        case VK_DELETE:
            k.key = KeyEvent::Key::Delete;
            break;
        case VK_LEFT:
            k.key = KeyEvent::Key::Left;
            break;
        case VK_RIGHT:
            k.key = KeyEvent::Key::Right;
            break;
        case VK_UP:
            k.key = KeyEvent::Key::Up;
            break;
        case VK_DOWN:
            k.key = KeyEvent::Key::Down;
            break;
        case VK_HOME:
            k.key = KeyEvent::Key::Home;
            break;
        case VK_END:
            k.key = KeyEvent::Key::End;
            break;
        case VK_TAB:
            k.key = KeyEvent::Key::Tab;
            break;
        default:
            // ctrl+letter (without alt, which is a layout's typing) is a shortcut: a Char with ctrl set.
            if (k.ctrl && !(GetKeyState(VK_MENU) & 0x8000) && wp >= 'A' && wp <= 'Z') {
                k.key = KeyEvent::Key::Char;
                k.text[0] = static_cast<char>(wp - 'A' + 'a');
            }
            break;
        }
        if (ev && k.key != KeyEvent::Key::Unknown)
            ev->onKey(k);
        return 0;
    }
    case WM_CHAR: {
        if (wp < 0x20 || wp == 0x7f)
            return 0;
        KeyEvent k;
        k.key = KeyEvent::Key::Char;
        const wchar_t wc = static_cast<wchar_t>(wp);
        WideCharToMultiByte(CP_UTF8, 0, &wc, 1, k.text, static_cast<int>(sizeof(k.text)) - 1, nullptr, nullptr);
        if (ev)
            ev->onKey(k);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (self && !self->trackedLeave) {
            TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&t);
            self->trackedLeave = true;
        }
        if (ev)
            ev->onMouseMove(x, y, (wp & MK_LBUTTON) != 0, (wp & MK_SHIFT) != 0);
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        if (ev)
            ev->onMouseUp(x, y, MouseButton::Left);
        return 0;
    case WM_RBUTTONUP:
        if (ev)
            ev->onMouseUp(x, y, MouseButton::Right);
        return 0;
    case WM_DROPFILES: {
        auto* drop = reinterpret_cast<HDROP>(wp);
        std::vector<std::filesystem::path> files;
        const UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < n; ++i) {
            std::wstring name(DragQueryFileW(drop, i, nullptr, 0), L'\0');
            DragQueryFileW(drop, i, name.data(), static_cast<UINT>(name.size() + 1));
            files.emplace_back(name);
        }
        DragFinish(drop);
        if (ev && !files.empty())
            ev->onFilesDropped(files);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, kTimerId);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    try {
        return handle(hwnd, msg, wp, lp);
    } catch (...) {
        return DefWindowProcW(hwnd, msg, wp, lp); // never unwind through the OS
    }
}

} // namespace

bool NativeWindow::supports(WindowApi api) { return api == WindowApi::Win32; }
std::unique_ptr<NativeWindow> NativeWindow::create() { return std::make_unique<Win32Window>(); }

} // namespace spux::inline v1::platform
#endif
