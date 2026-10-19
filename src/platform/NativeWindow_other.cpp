// Platforms without a native window implementation yet (Cocoa is the open one, as in Acidus):
// the plugin runs without an editor there.
#if !defined(_WIN32) && !(defined(__linux__) && !defined(__APPLE__))
#include "spux/platform/NativeWindow.hpp"
#include <memory>

namespace spux::inline v1::platform {

bool NativeWindow::supports(WindowApi) { return false; }
std::unique_ptr<NativeWindow> NativeWindow::create() { return nullptr; }

} // namespace spux::inline v1::platform
#endif
