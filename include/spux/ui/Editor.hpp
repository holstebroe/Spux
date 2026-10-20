// The editor shell (from Plumeflow `ui/Editor`): a native child window, a renderer and the Gui, drawn
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
// Not yet: the GPU path (a GL context and a renderer without layers, step 7) and window scaling; a
// CPU renderer draws at the logical size.
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "spux/platform/NativeWindow.hpp"
#include "spux/ui/Gui.hpp"

namespace spux::inline v1::ui {

struct EditorOptions {
    int width = 640; // logical size
    int height = 400;
    int activeHz = 60;
    int idleHz = 20;
    double activeHold = 1.5; // seconds after the last input the editor stays at activeHz
    bool caching = true;     // background layer and per-widget skip (CPU renderers)
    bool keyboard = true;    // the window takes key presses (WindowOptions::keyboard)
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
};

template <class G> class RootView : public RootViewBase {
public:
    // Panels and printed labels, drawn into the background layer (Gui in background mode).
    virtual void background(G& /*g*/) {}
    // The widgets, every frame.
    virtual void frame(G& g) = 0;
};

using RendererFactory = std::function<std::unique_ptr<IRenderer>(int width, int height)>;

class EditorCore : private platform::WindowEvents {
public:
    EditorCore(EditorHost& host, GuiCore& gui, RootViewBase& root, const EditorOptions& options,
               RendererFactory renderer);
    ~EditorCore() override;
    EditorCore(const EditorCore&) = delete;
    EditorCore& operator=(const EditorCore&) = delete;

    // --- window ---
    bool setParent(const platform::ParentWindow& parent);
    void show();
    void hide();
    void destroy();
    [[nodiscard]] int width() const { return options_.width; }
    [[nodiscard]] int height() const { return options_.height; }
    // The CPU renderers draw at the logical size: any other size is refused.
    bool setSize(int width, int height) const { return width == options_.width && height == options_.height; }

    // --- frames ---
    // Draws a frame when something changed (or always with force); true when it drew. `now` in seconds
    // on the editor's clock; < 0 reads the steady clock (tests pass their own time).
    bool frame(bool force = false, double now = -1.0);
    // The last frame, width() x height() opaque ARGB, and the part of it the last frame changed.
    [[nodiscard]] const std::uint32_t* pixels() const { return pixels_.data(); }
    [[nodiscard]] Rect lastDirty() const { return lastDirty_; }
    [[nodiscard]] int tickRate() const { return tickHz_; }
    [[nodiscard]] IRenderer* renderer() { return renderer_.get(); }
    [[nodiscard]] std::uint64_t framesDrawn() const { return framesDrawn_; }

    // --- input in logical pixels (the window's events land here; tests call them directly) ---
    void mouseDown(float x, float y, MouseButton b, bool shift);
    void mouseUp(float x, float y, MouseButton b);
    void mouseMove(float x, float y, bool shift = false);
    void mouseWheel(float x, float y, float steps, bool shift);
    void mouseLeave();
    void key(const KeyEvent& k);

protected:
    virtual void drawBackground() = 0;
    virtual void drawFrame() = 0;

private:
    void onTick() override { frame(); }
    void onExpose() override { frame(true); }
    void onMouseDown(int x, int y, MouseButton b, bool shift) override;
    void onMouseUp(int x, int y, MouseButton b) override;
    void onMouseMove(int x, int y, bool left, bool shift) override;
    void onMouseWheel(int x, int y, float steps, bool shift) override;
    void onMouseLeave() override;
    void onKey(const KeyEvent& k) override;

    [[nodiscard]] double clockNow() const;
    void touch();
    bool frameLocked(bool force, double now);

    EditorHost& host_;
    GuiCore& gui_;
    RootViewBase& root_;
    EditorOptions options_;
    RendererFactory makeRenderer_;
    mutable std::mutex mutex_;
    std::unique_ptr<platform::NativeWindow> window_;
    std::unique_ptr<IRenderer> renderer_;
    std::vector<std::uint32_t> pixels_;
    Rect lastDirty_;
    bool visible_ = false, first_ = true, input_ = false;
    std::uint32_t seenParams_ = 0;
    std::uint64_t seenContent_ = 0, seenBackground_ = 0;
    std::chrono::steady_clock::time_point start_;
    double last_ = 0.0, lastInput_ = -1e9;
    int tickHz_ = 0;
    std::uint64_t framesDrawn_ = 0;
};

template <class Style> class Editor final : public EditorCore {
public:
    using GuiType = Gui<Style>;
    Editor(EditorHost& host, const Style& style, RootView<GuiType>& root, const EditorOptions& options,
           RendererFactory renderer)
        : EditorCore(host, gui_, root, options, std::move(renderer)), gui_(host, style), root_(root) {}
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
