// Platforms without a GL context implementation: the editor uses the software renderer.
#include "spux/gl/GlContext.hpp"

#if !defined(_WIN32) && !(defined(__linux__) && !defined(__APPLE__))
namespace spux::inline v1::gl {
std::unique_ptr<GlContext> GlContext::createForWindow(const ParentWindow&, std::string* error) {
    if (error)
        *error = "no OpenGL context code for this platform";
    return nullptr;
}
std::unique_ptr<GlContext> GlContext::createOffscreen(int, int, std::string* error) {
    if (error)
        *error = "no OpenGL context code for this platform";
    return nullptr;
}
} // namespace spux::inline v1::gl
#endif
