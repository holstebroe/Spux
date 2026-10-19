// X11 only: picks the visual a GL window must be created with (shared by the window and the context code).
#pragma once
#if defined(__linux__) && !defined(__APPLE__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace spux::inline v1::platform {

// A double-buffered RGBA8 visual that GLX can render into. False when libGL or GLX is not usable.
bool glxChooseWindowVisual(Display* display, int screen, XVisualInfo& out);

} // namespace spux::inline v1::platform
#endif
