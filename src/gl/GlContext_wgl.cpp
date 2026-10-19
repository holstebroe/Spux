// WGL context (Windows). NOT YET COMPILED OR RUN: this machine has no Windows toolchain; the first CI
// run on windows-latest is the check. opengl32.dll is loaded explicitly, so a machine without a usable
// OpenGL driver gets no context and the editor falls back to the software renderer.
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "spux/gl/GlContext.hpp"

namespace spux::inline v1::gl {
namespace {

constexpr int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091, WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092,
              WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126, WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 1;

using CreateContextAttribsFn = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
using SwapIntervalFn = BOOL(WINAPI*)(int);

constexpr wchar_t kHiddenClass[] = L"SpuxGlHidden";

class WglContext final : public GlContext {
public:
    ~WglContext() override {
        if (ctx_) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(ctx_);
        }
        if (hwnd_ && hdc_)
            ReleaseDC(hwnd_, hdc_);
        if (ownWindow_ && hwnd_)
            DestroyWindow(hwnd_);
        if (lib_)
            FreeLibrary(lib_);
    }

    bool init(HWND window, int w, int h, std::string* error) {
        auto fail = [&](const char* why) {
            if (error)
                *error = why;
            return false;
        };
        lib_ = LoadLibraryW(L"opengl32.dll");
        if (!lib_)
            return fail("opengl32.dll not available");
        hwnd_ = window;
        if (!hwnd_) {
            HINSTANCE inst = GetModuleHandleW(nullptr);
            WNDCLASSW wc = {};
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = inst;
            wc.lpszClassName = kHiddenClass;
            wc.style = CS_OWNDC;
            RegisterClassW(&wc); // fails harmlessly when it exists already
            hwnd_ = CreateWindowExW(0, kHiddenClass, L"", WS_POPUP, 0, 0, w, h, nullptr, nullptr, inst, nullptr);
            ownWindow_ = true;
            if (!hwnd_)
                return fail("cannot create the offscreen window");
        }
        hdc_ = GetDC(hwnd_);
        if (!hdc_)
            return fail("no device context");

        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cAlphaBits = 0;
        pfd.cDepthBits = 0;
        const int format = ChoosePixelFormat(hdc_, &pfd);
        if (format == 0 || !SetPixelFormat(hdc_, format, &pfd))
            return fail("no pixel format");

        // a legacy context first: wglCreateContextAttribsARB can only be asked for with one current
        HGLRC legacy = wglCreateContext(hdc_);
        if (!legacy || !wglMakeCurrent(hdc_, legacy)) {
            if (legacy)
                wglDeleteContext(legacy);
            return fail("cannot create a legacy OpenGL context");
        }
        auto create = reinterpret_cast<CreateContextAttribsFn>(wglGetProcAddress("wglCreateContextAttribsARB"));
        swapInterval_ = reinterpret_cast<SwapIntervalFn>(wglGetProcAddress("wglSwapIntervalEXT"));
        const int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB,    3, WGL_CONTEXT_MINOR_VERSION_ARB, 3, WGL_CONTEXT_PROFILE_MASK_ARB,
            WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 0};
        ctx_ = create ? create(hdc_, nullptr, attribs) : nullptr;
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(legacy);
        if (!ctx_)
            return fail("cannot create an OpenGL 3.3 core context");
        return true;
    }

    bool makeCurrent() override { return wglMakeCurrent(hdc_, ctx_) != FALSE; }
    void doneCurrent() override { wglMakeCurrent(nullptr, nullptr); }
    void swapBuffers() override {
        if (!ownWindow_)
            SwapBuffers(hdc_);
    }
    void setSwapInterval(int interval) override {
        if (swapInterval_)
            swapInterval_(interval);
    }
    void* getProc(const char* name) override {
        void* p = reinterpret_cast<void*>(wglGetProcAddress(name));
        const auto v = reinterpret_cast<std::intptr_t>(p);
        if (p && v != 1 && v != 2 && v != 3 && v != -1)
            return p;
        return reinterpret_cast<void*>(GetProcAddress(lib_, name)); // the 1.1 functions
    }

private:
    HMODULE lib_ = nullptr;
    HWND hwnd_ = nullptr;
    HDC hdc_ = nullptr;
    HGLRC ctx_ = nullptr;
    SwapIntervalFn swapInterval_ = nullptr;
    bool ownWindow_ = false;
};

} // namespace

std::unique_ptr<GlContext> GlContext::createForWindow(const ParentWindow& window, std::string* error) {
    if (window.api != platform::WindowApi::Win32 || window.handle == 0) {
        if (error)
            *error = "not a Win32 window";
        return nullptr;
    }
    auto c = std::make_unique<WglContext>();
    if (!c->init(reinterpret_cast<HWND>(window.handle), 0, 0, error))
        return nullptr;
    return c;
}

std::unique_ptr<GlContext> GlContext::createOffscreen(int width, int height, std::string* error) {
    auto c = std::make_unique<WglContext>();
    if (!c->init(nullptr, width > 0 ? width : 1, height > 0 ? height : 1, error))
        return nullptr;
    return c;
}

} // namespace spux::inline v1::gl
#endif
