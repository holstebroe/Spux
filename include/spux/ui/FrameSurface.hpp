// How a frame reaches the screen when it is not a block of pixels handed to the window: a GPU context
// bound to the window. The editor shell (Editor.hpp) knows only this interface, so spux_ui_core needs no
// graphics library; a flavor implements it (ModernSurface for the GPU renderer).
//
// The editor opens the window with a GL pixel format, calls attach(), and from then on brackets every
// frame with beginFrame() and endFrame(). If attach() fails the editor closes that window, opens a plain
// one and draws with its CPU renderer: the plugin never sees the difference except through
// EditorCore::usesGpu().
//
// Threads: beginFrame, endFrame and resize run on the window's tick thread or the host's main thread, never
// at once (the editor's lock); a context is made current for the length of each call and released again.
// detach runs on the main thread with the window still open.
#pragma once

#include <string>

#include "spux/platform/NativeWindow.hpp"
#include "spux/ui/IRenderer.hpp"

namespace spux::inline v1::ui {

class FrameSurface {
public:
    virtual ~FrameSurface() = default;

    // The window was just opened for drawing with a GPU. `scale` is window pixels per logical pixel.
    // False with a reason when the surface cannot be used (no GL, too old, a software rasterizer).
    virtual bool attach(platform::NativeWindow& window, int logicalWidth, int logicalHeight, float scale,
                        std::string* why) = 0;
    // The logical size or the scale changed; the window has already been resized.
    virtual bool resize(int logicalWidth, int logicalHeight, float scale) = 0;
    // The renderer the page draws through; valid between attach and detach.
    [[nodiscard]] virtual IRenderer* renderer() = 0;
    // A frame: the renderer draws between beginFrame and endFrame, which presents it. beginFrame returns
    // false when the frame must be skipped.
    virtual bool beginFrame(double now) = 0;
    virtual void endFrame() = 0;
    // Frees every GPU object, with the window still open. Safe to call twice.
    virtual void detach() = 0;

    // What the surface draws with, for diagnostics ("NVIDIA GeForce ...").
    [[nodiscard]] virtual const char* name() const = 0;
    // 0: full quality; larger: a quality governor has reduced the effects. A plugin may thin its own detail.
    [[nodiscard]] virtual int degradation() const { return 0; }
};

} // namespace spux::inline v1::ui
