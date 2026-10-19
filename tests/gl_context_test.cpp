// GL parts of spux_ui_modern: the function table loads from an offscreen context, and a context can be
// attached to a native window opened for OpenGL. Both skip (and pass) where the machine has no usable
// OpenGL 3.3, or no X display for the window case: the editor falls back to the software renderer there.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "doctest.h"
#include "spux/gl/GlApi.hpp"
#include "spux/gl/GlContext.hpp"
#include "spux/platform/NativeWindow.hpp"

using namespace spux;

namespace {
// Clears a texture-backed framebuffer to a colour and reads one pixel back. Mesa's software rasteriser
// writes a pbuffer's default framebuffer with PutImage, which an Xvfb server may refuse, so the offscreen
// case renders into a framebuffer object, as the GPU renderer does.
void clearAndRead(gl::GlApi& api, int w, int h) {
    gl::GLuint tex = 0, fbo = 0;
    api.GenTextures(1, &tex);
    api.BindTexture(gl::TEXTURE_2D, tex);
    api.TexImage2D(gl::TEXTURE_2D, 0, static_cast<gl::GLint>(gl::RGBA8), w, h, 0, gl::RGBA, gl::UNSIGNED_BYTE, nullptr);
    api.GenFramebuffers(1, &fbo);
    api.BindFramebuffer(gl::FRAMEBUFFER, fbo);
    api.FramebufferTexture2D(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0, gl::TEXTURE_2D, tex, 0);
    REQUIRE(api.CheckFramebufferStatus(gl::FRAMEBUFFER) == gl::FRAMEBUFFER_COMPLETE);
    api.Viewport(0, 0, w, h);
    api.ClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    api.Clear(gl::COLOR_BUFFER_BIT);
    unsigned char px[4] = {};
    api.ReadPixels(w / 2, h / 2, 1, 1, gl::RGBA, gl::UNSIGNED_BYTE, px);
    CHECK(px[0] == 0);
    CHECK(px[1] == 255);
    CHECK(px[2] == 0);
    api.BindFramebuffer(gl::FRAMEBUFFER, 0);
    api.DeleteFramebuffers(1, &fbo);
    api.DeleteTextures(1, &tex);
}
} // namespace

TEST_CASE("offscreen context: function table loads and renders") {
    std::string why;
    auto ctx = gl::GlContext::createOffscreen(64, 48, &why);
    if (!ctx) {
        std::printf("gl_context_test: no offscreen context (%s), skipped\n", why.c_str());
        return;
    }
    REQUIRE(ctx->makeCurrent());
    gl::GlApi api;
    REQUIRE(ctx->loadApi(api));
    CHECK((api.major > 3 || (api.major == 3 && api.minor >= 3)));
    CHECK(api.version[0] != 0);
    clearAndRead(api, 64, 48);
    ctx->doneCurrent();
}

#if defined(SPUX_HAS_X11)
TEST_CASE("window context: a GL window takes a context and presents") {
    if (!std::getenv("DISPLAY"))
        return;
    using namespace spux::platform;
    struct Sink : WindowEvents {
    } sink;
    auto win = NativeWindow::create();
    REQUIRE(win);
    WindowOptions opt;
    opt.openGl = true;
    bool opened = false;
    for (int i = 0; i < 20 && !opened; ++i) { // the X server can refuse a connection for a moment under load
        opened = win->open({WindowApi::X11, 0}, 80, 60, &sink, opt);
        if (!opened)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (!opened) {
        std::printf("gl_context_test: no GL visual, skipped\n");
        return;
    }
    // a GL window shows nothing through present(): it must be ignored, not crash
    std::uint32_t px[4] = {};
    win->present(px, 2, 2, {0, 0, 2, 2});

    std::string why;
    auto ctx = gl::GlContext::createForWindow(win->nativeHandle(), &why);
    if (!ctx) {
        std::printf("gl_context_test: no window context (%s), skipped\n", why.c_str());
        win->close();
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // let the window be mapped
    REQUIRE(ctx->makeCurrent());
    gl::GlApi api;
    REQUIRE(ctx->loadApi(api));
    ctx->setSwapInterval(0);
    clearAndRead(api, 80, 60);
    ctx->swapBuffers();
    ctx->doneCurrent();
    ctx.reset(); // the context goes before its window
    win->close();
}
#endif
