// Platforms without a native window implementation yet (Cocoa is the open one, as in Acidus), and Linux
// builds with SPUX_WITH_X11=OFF: the plugin runs without an editor there.
#if !defined(_WIN32) && !defined(SPUX_HAS_X11)
#include "spux/platform/NativeWindow.hpp"
#include <memory>

namespace spux::inline v1::platform {

bool NativeWindow::supports(WindowApi) { return false; }
std::unique_ptr<NativeWindow> NativeWindow::create() { return nullptr; }

} // namespace spux::inline v1::platform
#endif
