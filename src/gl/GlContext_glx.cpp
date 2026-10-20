// GLX context (X11). libGL is opened with dlopen so the plugin loads on machines without it. Each
// context has its own Display connection, so the editor thread's Xlib use and the render thread never
// share one, and instances do not interfere.
#if defined(SPUX_HAS_X11)
#include <dlfcn.h>

#include <atomic>
#include <cstring>
#include <mutex>

#include "spux/gl/GlContext.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace spux::inline v1::gl {
namespace {

using FBConfig = void*;
using Context = void*;
using Drawable = unsigned long;

constexpr int GLX_DOUBLEBUFFER = 5, GLX_RED_SIZE = 8, GLX_GREEN_SIZE = 9, GLX_BLUE_SIZE = 10, GLX_ALPHA_SIZE = 11,
              GLX_DRAWABLE_TYPE = 0x8010, GLX_RENDER_TYPE = 0x8011, GLX_X_RENDERABLE = 0x8012, GLX_VISUAL_ID = 0x800B,
              GLX_WINDOW_BIT = 1, GLX_PBUFFER_BIT = 4, GLX_RGBA_BIT = 1, GLX_PBUFFER_WIDTH = 0x8041,
              GLX_PBUFFER_HEIGHT = 0x8040, GLX_CONTEXT_MAJOR_VERSION_ARB = 0x2091,
              GLX_CONTEXT_MINOR_VERSION_ARB = 0x2092, GLX_CONTEXT_PROFILE_MASK_ARB = 0x9126,
              GLX_CONTEXT_CORE_PROFILE_BIT_ARB = 1;

struct Glx {
    void* lib = nullptr;
    FBConfig* (*chooseFBConfig)(Display*, int, const int*, int*) = nullptr;
    int (*getFBConfigAttrib)(Display*, FBConfig, int, int*) = nullptr;
    XVisualInfo* (*getVisualFromFBConfig)(Display*, FBConfig) = nullptr;
    Context (*createContextAttribs)(Display*, FBConfig, Context, int, const int*) = nullptr;
    void (*destroyContext)(Display*, Context) = nullptr;
    int (*makeCurrent)(Display*, Drawable, Context) = nullptr;
    void (*swapBuffers)(Display*, Drawable) = nullptr;
    Drawable (*createPbuffer)(Display*, FBConfig, const int*) = nullptr;
    void (*destroyPbuffer)(Display*, Drawable) = nullptr;
    void* (*getProcAddress)(const unsigned char*) = nullptr;
    int (*swapIntervalExt)(Display*, Drawable, int) = nullptr; // optional
    int (*swapIntervalMesa)(int) = nullptr;                    // optional
    bool ok = false;
};

const Glx& glx() {
    static Glx g;
    static std::once_flag once;
    std::call_once(once, [] {
        void* lib = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
        if (!lib)
            lib = dlopen("libGL.so", RTLD_LAZY | RTLD_LOCAL);
        if (!lib)
            return;
        g.lib = lib;
        auto sym = [&](const char* n) { return dlsym(lib, n); };
        g.chooseFBConfig = reinterpret_cast<decltype(g.chooseFBConfig)>(sym("glXChooseFBConfig"));
        g.getFBConfigAttrib = reinterpret_cast<decltype(g.getFBConfigAttrib)>(sym("glXGetFBConfigAttrib"));
        g.getVisualFromFBConfig = reinterpret_cast<decltype(g.getVisualFromFBConfig)>(sym("glXGetVisualFromFBConfig"));
        g.destroyContext = reinterpret_cast<decltype(g.destroyContext)>(sym("glXDestroyContext"));
        g.makeCurrent = reinterpret_cast<decltype(g.makeCurrent)>(sym("glXMakeCurrent"));
        g.swapBuffers = reinterpret_cast<decltype(g.swapBuffers)>(sym("glXSwapBuffers"));
        g.createPbuffer = reinterpret_cast<decltype(g.createPbuffer)>(sym("glXCreatePbuffer"));
        g.destroyPbuffer = reinterpret_cast<decltype(g.destroyPbuffer)>(sym("glXDestroyPbuffer"));
        g.getProcAddress = reinterpret_cast<decltype(g.getProcAddress)>(sym("glXGetProcAddressARB"));
        if (!g.getProcAddress)
            g.getProcAddress = reinterpret_cast<decltype(g.getProcAddress)>(sym("glXGetProcAddress"));
        if (!g.getProcAddress)
            return;
        g.createContextAttribs = reinterpret_cast<decltype(g.createContextAttribs)>(
            g.getProcAddress(reinterpret_cast<const unsigned char*>("glXCreateContextAttribsARB")));
        g.swapIntervalExt = reinterpret_cast<decltype(g.swapIntervalExt)>(
            g.getProcAddress(reinterpret_cast<const unsigned char*>("glXSwapIntervalEXT")));
        g.swapIntervalMesa = reinterpret_cast<decltype(g.swapIntervalMesa)>(
            g.getProcAddress(reinterpret_cast<const unsigned char*>("glXSwapIntervalMESA")));
        g.ok = g.chooseFBConfig && g.getFBConfigAttrib && g.getVisualFromFBConfig && g.destroyContext &&
               g.makeCurrent && g.swapBuffers && g.createPbuffer && g.destroyPbuffer && g.createContextAttribs;
    });
    return g;
}

// An X error during context creation (BadValue, GLXBadFBConfig...) would terminate the host with Xlib's
// default handler: catch it for the duration of the calls that can raise one.
std::atomic<bool> gXError{false};
int errorHandler(Display*, XErrorEvent*) {
    gXError = true;
    return 0;
}
struct ErrorScope {
    Display* d;
    int (*old)(Display*, XErrorEvent*);
    explicit ErrorScope(Display* dpy) : d(dpy) {
        gXError = false;
        old = XSetErrorHandler(errorHandler);
    }
    bool failed() {
        XSync(d, False);
        return gXError.load();
    }
    ~ErrorScope() {
        XSync(d, False);
        XSetErrorHandler(old);
    }
};

class GlxContext final : public GlContext {
public:
    ~GlxContext() override {
        if (!display_)
            return;
        const Glx& g = glx();
        if (ctx_) {
            g.makeCurrent(display_, 0, nullptr);
            g.destroyContext(display_, ctx_);
        }
        if (pbuffer_)
            g.destroyPbuffer(display_, pbuffer_);
        XCloseDisplay(display_);
    }

    bool init(Window window, int pbufferW, int pbufferH, std::string* error) {
        const Glx& g = glx();
        auto fail = [&](const char* why) {
            if (error)
                *error = why;
            return false;
        };
        if (!g.ok)
            return fail("libGL/GLX not available");
        display_ = XOpenDisplay(nullptr);
        if (!display_)
            return fail("cannot open the X display");
        const int screen = DefaultScreen(display_);

        if (window) {
            XWindowAttributes wa;
            if (!XGetWindowAttributes(display_, window, &wa))
                return fail("window attributes");
            const int visualId = static_cast<int>(XVisualIDFromVisual(wa.visual));
            const int attribs[] = {GLX_X_RENDERABLE,
                                   1,
                                   GLX_DRAWABLE_TYPE,
                                   GLX_WINDOW_BIT,
                                   GLX_RENDER_TYPE,
                                   GLX_RGBA_BIT,
                                   GLX_DOUBLEBUFFER,
                                   1,
                                   0};
            int total = 0;
            FBConfig* all = g.chooseFBConfig(display_, screen, attribs, &total);
            for (int i = 0; i < total && !config_; ++i) {
                int id = 0;
                g.getFBConfigAttrib(display_, all[i], GLX_VISUAL_ID, &id);
                if (id == visualId)
                    config_ = all[i];
            }
            if (all)
                XFree(all);
            if (!config_)
                return fail("no GLX framebuffer configuration for the window's visual");
        } else {
            const int attribs[] = {GLX_X_RENDERABLE,
                                   1,
                                   GLX_DRAWABLE_TYPE,
                                   GLX_PBUFFER_BIT,
                                   GLX_RENDER_TYPE,
                                   GLX_RGBA_BIT,
                                   GLX_RED_SIZE,
                                   8,
                                   GLX_GREEN_SIZE,
                                   8,
                                   GLX_BLUE_SIZE,
                                   8,
                                   GLX_ALPHA_SIZE,
                                   8,
                                   0};
            int n = 0;
            FBConfig* configs = g.chooseFBConfig(display_, screen, attribs, &n);
            if (configs && n > 0)
                config_ = configs[0];
            if (configs)
                XFree(configs);
            if (!config_)
                return fail("no pbuffer framebuffer configuration");
        }

        ErrorScope scope(display_);
        const int ctxAttribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB,    3, GLX_CONTEXT_MINOR_VERSION_ARB, 3, GLX_CONTEXT_PROFILE_MASK_ARB,
            GLX_CONTEXT_CORE_PROFILE_BIT_ARB, 0};
        ctx_ = g.createContextAttribs(display_, config_, nullptr, 1, ctxAttribs);
        if (scope.failed() || !ctx_) {
            ctx_ = nullptr;
            return fail("cannot create an OpenGL 3.3 core context");
        }
        if (window) {
            drawable_ = window;
        } else {
            const int pb[] = {GLX_PBUFFER_WIDTH, pbufferW, GLX_PBUFFER_HEIGHT, pbufferH, 0};
            pbuffer_ = g.createPbuffer(display_, config_, pb);
            if (scope.failed() || !pbuffer_)
                return fail("cannot create a pbuffer");
            drawable_ = pbuffer_;
        }
        return true;
    }

    // No error handler here: XSetErrorHandler is process-wide and these run every frame, next to the host's
    // own X use. Only creation, which can raise errors for unsupported requests, is guarded.
    bool makeCurrent() override { return glx().makeCurrent(display_, drawable_, ctx_) != 0; }
    void doneCurrent() override { glx().makeCurrent(display_, 0, nullptr); }
    void swapBuffers() override {
        if (!pbuffer_)
            glx().swapBuffers(display_, drawable_);
    }
    void setSwapInterval(int interval) override {
        const Glx& g = glx();
        if (g.swapIntervalExt && !pbuffer_)
            g.swapIntervalExt(display_, drawable_, interval);
        else if (g.swapIntervalMesa)
            g.swapIntervalMesa(interval);
    }
    void* getProc(const char* name) override {
        const Glx& g = glx();
        void* p = g.getProcAddress ? g.getProcAddress(reinterpret_cast<const unsigned char*>(name)) : nullptr;
        return p ? p : dlsym(g.lib, name);
    }

private:
    Display* display_ = nullptr;
    FBConfig config_ = nullptr;
    Context ctx_ = nullptr;
    Drawable drawable_ = 0, pbuffer_ = 0;
};

} // namespace

std::unique_ptr<GlContext> GlContext::createForWindow(const ParentWindow& window, std::string* error) {
    if (window.api != platform::WindowApi::X11 || window.handle == 0) {
        if (error)
            *error = "not an X11 window";
        return nullptr;
    }
    auto c = std::make_unique<GlxContext>();
    if (!c->init(static_cast<Window>(window.handle), 0, 0, error))
        return nullptr;
    return c;
}

std::unique_ptr<GlContext> GlContext::createOffscreen(int width, int height, std::string* error) {
    auto c = std::make_unique<GlxContext>();
    if (!c->init(0, width > 0 ? width : 1, height > 0 ? height : 1, error))
        return nullptr;
    return c;
}

} // namespace spux::inline v1::gl
#endif
