// The editor shell: a native child window, a renderer and the Gui, drawn
// only when something changed (guidelines 7, immediate mode cache 1). The plugin gives it the logical
// size and a root view; the flavor gives it the renderer.
//
// Frames: on every window tick the editor draws if there was input, the parameter version or the root
// view's content version changed, the root view animates, or the Gui waits for something (a tooltip
// timer, a caret, a drag); otherwise the tick costs a few comparisons. The tick runs at activeHz while
// the user works or something animates, at idleHz otherwise, and not at all while the window is hidden.
//
// The background (RootView::background: panels, printed labels) is drawn into the renderer's background
// layer when its key changes; RootView::frame draws the widgets every frame, and the caches skip what
// did not change. Without a window (tests, headless benchmarks) frame() draws into pixels().
//
// Two ways to draw. A CPU renderer (the factory) draws at the logical size into pixels the editor blits;
// the window has that size. With a FrameSurface (FrameSurface.hpp) the editor opens a GL window and the
// surface presents each frame itself; the renderer has no layers, so every widget draws every frame; the
// window is the logical size times the scale: the user's scale (1 by default, within the options'
// limits, from the host's resizing or the plugin) times the system's (HiDPI). If the surface cannot be
// attached the editor falls back to the CPU renderer without a word: usesGpu() tells.
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "spux/platform/NativeWindow.hpp"
#include "spux/ui/FrameSurface.hpp"
#include "spux/ui/Gui.hpp"

namespace spux::inline v1::ui {

struct EditorOptions {
    int width = 640; // logical size
    int height = 400;
    int activeHz = 60;
    int idleHz = 20;
    double activeHold = 1.5;  // seconds after the last input the editor stays at activeHz
    bool caching = true;      // background layer and per-widget skip (CPU renderers)
    bool keyboard = true;     // the window takes key presses (WindowOptions::keyboard)
    bool acceptFiles = false; // files dropped on the window reach RootViewBase::filesDropped
    // With a FrameSurface: the user's scale and its limits; the window is `width * scale` wide, at a fixed
    // aspect ratio. A CPU renderer ignores them.
    float scale = 1.0f;
    float minScale = 0.75f;
    float maxScale = 2.0f;
};

// What the editor asks the plugin's root view, apart from drawing.
class RootViewBase {
public:
    virtual ~RootViewBase() = default;
    // The background is redrawn when this changes (a page switch, a theme change).
    [[nodiscard]] virtual std::uint64_t backgroundKey() const { return 0; }
    // Changes when something the page shows changed outside the parameters (a preset name, held notes,
    // new telemetry): the next tick draws a frame.
    [[nodiscard]] virtual std::uint64_t contentVersion() { return 0; }
    // True while an animation runs: every tick draws.
    [[nodiscard]] virtual bool animating() { return false; }
    // Every tick, before the editor decides whether to draw (poll a file dialog, advance a timer).
    virtual void tick(double /*now*/) {}
    // Files dropped on the window (EditorOptions::acceptFiles), in the order the file manager gives them.
    virtual void filesDropped(const std::vector<std::filesystem::path>& /*files*/) {}
};

template <class G> class RootView : public RootViewBase {
public:
    // Panels and printed labels, drawn into the background layer (Gui in background mode).
    virtual void background(G& /*g*/) {}
    // The widgets, every frame.
    virtual void frame(G& g) = 0;
};

using RendererFactory = std::function<std::unique_ptr<IRenderer>(int width, int height)>;
// Makes the surface to try before the CPU renderer; null: no GPU path.
using SurfaceFactory = std::function<std::unique_ptr<FrameSurface>()>;

class EditorCore : private platform::WindowEvents {
public:
    EditorCore(EditorHost& host, GuiCore& gui, RootViewBase& root, const EditorOptions& options,
               RendererFactory renderer, SurfaceFactory surface = {});
    ~EditorCore() override;
    EditorCore(const EditorCore&) = delete;
    EditorCore& operator=(const EditorCore&) = delete;

    // --- window ---
    bool setParent(const platform::ParentWindow& parent);
    void show();
    void hide();
    void destroy();
    // The window's size in pixels as the host sees it: the logical size, times the scale with a GPU surface.
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    // The host may resize the window only with a GPU surface: the CPU renderers draw at the logical size.
    [[nodiscard]] bool canResize() const;
    // The nearest size setSize would make of a requested one: the aspect ratio kept, the user scale within
    // its limits, on top of the system's scale. Unchanged logical size with a CPU renderer.
    void adjustSize(int& width, int& height) const;
    // A window size from the host (pixels). With a CPU renderer only the logical size is accepted.
    bool setSize(int width, int height);
    // The system's scale from the host (HiDPI); GPU surface only.
    bool setScale(double systemScale);
    // The user's scale (a scale control in the plugin's own UI); GPU surface only, clamped to the limits.
    bool setUserScale(float scale);
    [[nodiscard]] float userScale() const;
    // Window pixels per logical pixel (1 with a CPU renderer).
    [[nodiscard]] float scale() const;
    [[nodiscard]] bool usesGpu() const;
    // What draws: the surface's name, or "software" for a CPU renderer.
    [[nodiscard]] const char* rendererName() const;
    // How far a quality governor has reduced the GPU effects (0: none; 0 for a CPU renderer).
    [[nodiscard]] int degradation() const;

    // --- frames ---
    // Draws a frame when something changed (or always with force); true when it drew. `now` in seconds
    // on the editor's clock; < 0 reads the steady clock (tests pass their own time).
    bool frame(bool force = false, double now = -1.0);
    // CPU renderers only: the last frame, the logical size, opaque ARGB, and the part of it the last frame changed.
    [[nodiscard]] const std::uint32_t* pixels() const { return pixels_.data(); }
    [[nodiscard]] Rect lastDirty() const;
    [[nodiscard]] int tickRate() const { return tickHz_; }
    [[nodiscard]] IRenderer* renderer() { return renderer_.get(); }
    [[nodiscard]] std::uint64_t framesDrawn() const { return framesDrawn_.load(std::memory_order_relaxed); }

    // --- input in logical pixels (the window's events land here; tests call them directly) ---
    void mouseDown(float x, float y, MouseButton b, bool shift);
    void mouseUp(float x, float y, MouseButton b);
    void mouseMove(float x, float y, bool shift = false);
    void mouseWheel(float x, float y, float steps, bool shift);
    void mouseLeave();
    void key(const KeyEvent& k);
    void filesDropped(const std::vector<std::filesystem::path>& files);

protected:
    virtual void drawBackground() = 0;
    virtual void drawFrame() = 0;

private:
    void onTick() override { frame(); }
    void onExpose() override { frame(true); }
    void onMouseDown(int x, int y, MouseButton b, bool shift) override;
    void onMouseUp(int x, int y, MouseButton b) override;
    void onMouseMove(int x, int y, bool left, bool shift) override;
    void onCtrl(bool ctrl) override;
    void onMouseWheel(int x, int y, float steps, bool shift) override;
    void onMouseLeave() override;
    void onKey(const KeyEvent& k) override;
    void onFilesDropped(const std::vector<std::filesystem::path>& files) override { filesDropped(files); }

    [[nodiscard]] double clockNow() const;
    void touch();
    bool frameLocked(bool force, double now);
    [[nodiscard]] float scaleLocked() const { return surface_ ? userScale_ * systemScale_ : 1.0f; }
    [[nodiscard]] int physical(int logical) const;
    bool openWindow(const platform::ParentWindow& parent, bool gl, float scale);
    void applyScale(); // the window and the surface follow a changed scale

    EditorHost& host_;
    GuiCore& gui_;
    RootViewBase& root_;
    EditorOptions options_;
    RendererFactory makeRenderer_;
    SurfaceFactory makeSurface_;
    mutable std::mutex mutex_;
    std::unique_ptr<platform::NativeWindow> window_;
    std::unique_ptr<FrameSurface> surface_;
    std::unique_ptr<IRenderer> renderer_;
    float userScale_ = 1.0f, systemScale_ = 1.0f;
    bool scaleDirty_ = false;
    std::vector<std::uint32_t> pixels_;
    Rect lastDirty_;
    bool visible_ = false, first_ = true, input_ = false;
    std::atomic<bool> forcePending_{false}; // a forced frame that found the editor busy, owed to the next tick
    std::uint32_t seenParams_ = 0;
    std::uint64_t seenContent_ = 0, seenBackground_ = 0;
    std::chrono::steady_clock::time_point start_;
    double last_ = 0.0, lastInput_ = -1e9;
    int tickHz_ = 0;
    std::atomic<std::uint64_t> framesDrawn_{0};
};

template <class Style> class Editor final : public EditorCore {
public:
    using GuiType = Gui<Style>;
    Editor(EditorHost& host, const Style& style, RootView<GuiType>& root, const EditorOptions& options,
           RendererFactory renderer, SurfaceFactory surface = {})
        : EditorCore(host, gui_, root, options, std::move(renderer), std::move(surface)), gui_(host, style),
          root_(root) {}
    ~Editor() override { destroy(); }

    [[nodiscard]] GuiType& gui() { return gui_; }

private:
    void drawBackground() override { root_.background(gui_); }
    void drawFrame() override {
        root_.frame(gui_);
        gui_.end();
    }

    GuiType gui_; // the base only keeps a reference until it draws
    RootView<GuiType>& root_;
};

} // namespace spux::inline v1::ui
