// OpenGL context bound to a native window (or to nothing, for headless tests), spec 11. One context per
// editor instance; nothing is shared. The platform parts are WGL (Windows), GLX (X11) and, later, CGL.
// The OpenGL library is opened at run time, so a machine without it simply gets no GPU context and the
// editor falls back to the software renderer.
#pragma once
#include <memory>
#include <string>

#include "spux/gl/GlApi.hpp"
#include "spux/platform/NativeWindow.hpp"

namespace spux::inline v1::gl {

using platform::ParentWindow;

class GlContext {
public:
    virtual ~GlContext() = default;

    // A 3.3 core context drawing into `window` (opened with WindowOptions::openGl). nullptr and a reason
    // in *error when the platform has no context code, the library is missing or 3.3 is not available.
    static std::unique_ptr<GlContext> createForWindow(const ParentWindow& window, std::string* error = nullptr);
    // A context with a width x height default framebuffer that is not shown (tests, golden frames).
    static std::unique_ptr<GlContext> createOffscreen(int width, int height, std::string* error = nullptr);

    // Binds the context to the calling thread; a context is used from one thread at a time.
    virtual bool makeCurrent() = 0;
    virtual void doneCurrent() = 0;
    virtual void swapBuffers() = 0;
    virtual void setSwapInterval(int interval) = 0;
    virtual void* getProc(const char* name) = 0;

    // Loads the function table of this context; call with the context current.
    bool loadApi(GlApi& api) {
        return api.load([](void* self, const char* name) { return static_cast<GlContext*>(self)->getProc(name); },
                        this);
    }
};

} // namespace spux::inline v1::gl
