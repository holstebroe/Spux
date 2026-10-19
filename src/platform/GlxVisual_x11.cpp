// The GLX visual a GL window is created with (see GlxVisual.hpp). libGL is opened at run time and only
// two of its functions are used, so this adds no link dependency to a plugin that never opens a GL window.
#if defined(__linux__) && !defined(__APPLE__)
#include <dlfcn.h>

#include "GlxVisual.hpp"

namespace spux::inline v1::platform {

bool glxChooseWindowVisual(Display* display, int screen, XVisualInfo& out) {
    constexpr int GLX_DOUBLEBUFFER = 5, GLX_RED_SIZE = 8, GLX_GREEN_SIZE = 9, GLX_BLUE_SIZE = 10, GLX_ALPHA_SIZE = 11,
                  GLX_DEPTH_SIZE = 12, GLX_X_VISUAL_TYPE = 0x22, GLX_DRAWABLE_TYPE = 0x8010, GLX_RENDER_TYPE = 0x8011,
                  GLX_X_RENDERABLE = 0x8012, GLX_TRUE_COLOR = 0x8002, GLX_WINDOW_BIT = 1, GLX_RGBA_BIT = 1;
    using FBConfig = void*;
    if (!display)
        return false;
    void* lib = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!lib)
        lib = dlopen("libGL.so", RTLD_LAZY | RTLD_LOCAL);
    if (!lib)
        return false;
    // The handle stays open on purpose: the GL context code opens the same library, and a plugin that
    // got this far will use it.
    auto chooseFBConfig =
        reinterpret_cast<FBConfig* (*)(Display*, int, const int*, int*)>(dlsym(lib, "glXChooseFBConfig"));
    auto getVisualFromFBConfig =
        reinterpret_cast<XVisualInfo* (*)(Display*, FBConfig)>(dlsym(lib, "glXGetVisualFromFBConfig"));
    if (!chooseFBConfig || !getVisualFromFBConfig)
        return false;
    const int attribs[] = {GLX_X_RENDERABLE,
                           1,
                           GLX_DRAWABLE_TYPE,
                           GLX_WINDOW_BIT,
                           GLX_RENDER_TYPE,
                           GLX_RGBA_BIT,
                           GLX_X_VISUAL_TYPE,
                           GLX_TRUE_COLOR,
                           GLX_RED_SIZE,
                           8,
                           GLX_GREEN_SIZE,
                           8,
                           GLX_BLUE_SIZE,
                           8,
                           GLX_ALPHA_SIZE,
                           0,
                           GLX_DEPTH_SIZE,
                           0,
                           GLX_DOUBLEBUFFER,
                           1,
                           0};
    int n = 0;
    FBConfig* configs = chooseFBConfig(display, screen, attribs, &n);
    if (!configs || n == 0) {
        if (configs)
            XFree(configs);
        return false;
    }
    XVisualInfo* vi = getVisualFromFBConfig(display, configs[0]);
    XFree(configs);
    if (!vi)
        return false;
    out = *vi;
    XFree(vi);
    return true;
}

} // namespace spux::inline v1::platform
#endif
