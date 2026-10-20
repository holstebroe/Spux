#include "spux/ui/Editor.hpp"

#include <algorithm>

namespace spux::inline v1::ui {

EditorCore::EditorCore(EditorHost& host, GuiCore& gui, RootViewBase& root, const EditorOptions& options,
                       RendererFactory renderer)
    : host_(host), gui_(gui), root_(root), options_(options), makeRenderer_(std::move(renderer)) {
    options_.width = std::max(1, options_.width);
    options_.height = std::max(1, options_.height);
    pixels_.assign(static_cast<std::size_t>(options_.width) * static_cast<std::size_t>(options_.height), 0xFF000000u);
    start_ = std::chrono::steady_clock::now();
}

EditorCore::~EditorCore() { destroy(); }

double EditorCore::clockNow() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
}

bool EditorCore::setParent(const platform::ParentWindow& parent) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (window_ || !platform::NativeWindow::supports(parent.api))
        return false;
    auto w = platform::NativeWindow::create();
    platform::WindowOptions wo;
    wo.keyboard = options_.keyboard;
    if (!w || !w->open(parent, options_.width, options_.height, this, wo))
        return false;
    window_ = std::move(w);
    tickHz_ = options_.idleHz;
    window_->setTickRate(tickHz_);
    first_ = true;
    return true;
}

void EditorCore::show() {
    std::lock_guard<std::mutex> lock(mutex_);
    visible_ = true;
    host_.setTelemetryEnabled(true);
    if (window_)
        window_->setVisible(true);
    lastInput_ = clockNow();
    first_ = true; // the window's contents may be stale: present everything
}

void EditorCore::hide() {
    std::lock_guard<std::mutex> lock(mutex_);
    visible_ = false;
    host_.setTelemetryEnabled(false); // a hidden editor costs the audio thread nothing
    if (window_)
        window_->setVisible(false);
}

void EditorCore::destroy() {
    std::unique_ptr<platform::NativeWindow> w;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        host_.setTelemetryEnabled(false);
        visible_ = false;
        w = std::move(window_);
    }
    if (w)
        w->close(); // joins the event thread outside the lock: its callbacks take the lock
}

// --- input --------------------------------------------------------------------------------------------------

void EditorCore::touch() {
    input_ = true;
    lastInput_ = clockNow();
}

void EditorCore::mouseDown(float x, float y, MouseButton b, bool shift) {
    std::lock_guard<std::mutex> lock(mutex_);
    touch();
    gui_.onMouseDown(x, y, b, shift);
}
void EditorCore::mouseUp(float x, float y, MouseButton b) {
    std::lock_guard<std::mutex> lock(mutex_);
    touch();
    gui_.onMouseUp(x, y, b);
}
void EditorCore::mouseMove(float x, float y, bool shift) {
    std::lock_guard<std::mutex> lock(mutex_);
    touch();
    gui_.onMouseMove(x, y);
    gui_.setShift(shift);
}
void EditorCore::mouseWheel(float x, float y, float steps, bool shift) {
    std::lock_guard<std::mutex> lock(mutex_);
    touch();
    gui_.onWheel(x, y, steps, shift);
}
void EditorCore::mouseLeave() {
    std::lock_guard<std::mutex> lock(mutex_);
    touch();
    gui_.onLeave();
}
void EditorCore::key(const KeyEvent& k) {
    std::lock_guard<std::mutex> lock(mutex_);
    touch();
    gui_.onKey(k);
}

void EditorCore::onMouseDown(int x, int y, MouseButton b, bool shift) {
    mouseDown(static_cast<float>(x), static_cast<float>(y), b, shift);
}
void EditorCore::onMouseUp(int x, int y, MouseButton b) { mouseUp(static_cast<float>(x), static_cast<float>(y), b); }
void EditorCore::onMouseMove(int x, int y, bool, bool shift) {
    mouseMove(static_cast<float>(x), static_cast<float>(y), shift);
}
void EditorCore::onMouseWheel(int x, int y, float steps, bool shift) {
    mouseWheel(static_cast<float>(x), static_cast<float>(y), steps, shift);
}
void EditorCore::onMouseLeave() { mouseLeave(); }
void EditorCore::onKey(const KeyEvent& k) { key(k); }

// --- frames -------------------------------------------------------------------------------------------------

bool EditorCore::frame(bool force, double now) {
    // a tick never waits for the host's thread: it skips the frame while the editor is being changed
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock())
        return false;
    return frameLocked(force, now < 0.0 ? clockNow() : now);
}

bool EditorCore::frameLocked(bool force, double now) {
    if (window_ && !visible_)
        return false;
    if (!renderer_) {
        renderer_ = makeRenderer_ ? makeRenderer_(options_.width, options_.height) : nullptr;
        if (!renderer_)
            return false;
        first_ = true;
    }
    root_.tick(now);
    const std::uint32_t params = host_.paramVersion();
    const std::uint64_t content = root_.contentVersion();
    const std::uint64_t bgKey = root_.backgroundKey();
    const bool animating = root_.animating();
    const bool needed = force || first_ || input_ || params != seenParams_ || content != seenContent_ ||
                        bgKey != seenBackground_ || animating || gui_.wantsFrame(now);

    // the tick rate follows activity: fast while the user works or something moves
    if (window_) {
        const bool active = animating || now - lastInput_ < options_.activeHold || gui_.wantsFrame(now);
        const int hz = active ? options_.activeHz : options_.idleHz;
        if (hz != tickHz_) {
            tickHz_ = hz;
            window_->setTickRate(hz);
        }
    }
    if (!needed)
        return false;

    const float dt = static_cast<float>(std::clamp(now - last_, 0.0, 0.25));
    last_ = now;
    seenParams_ = params;
    seenContent_ = content;
    input_ = false;

    IRenderer& r = *renderer_;
    LayerCache* layers = r.layers();
    const bool cached = options_.caching && layers != nullptr;
    const bool fullBackground = first_ || bgKey != seenBackground_ || !cached;
    if (fullBackground) {
        if (layers)
            layers->setTarget(Layer::Background);
        r.clear(gui_.theme().colors.background);
        gui_.beginBackground(r, options_.width, options_.height);
        drawBackground();
        gui_.endBackground();
        if (layers) {
            layers->setTarget(Layer::Widgets);
            layers->restore({0, 0, options_.width, options_.height});
        }
        seenBackground_ = bgKey;
    }
    gui_.setCaching(cached);
    gui_.begin(r, now, dt, options_.width, options_.height);
    drawFrame();
    if (layers) {
        lastDirty_ = layers->dirtyBounds();
        if (first_)
            lastDirty_ = {0, 0, options_.width, options_.height};
        layers->resolve(pixels_.data());
        layers->clearDirty();
    } else {
        lastDirty_ = {0, 0, options_.width, options_.height};
    }
    first_ = false;
    ++framesDrawn_;
    if (window_ && !lastDirty_.empty())
        window_->present(pixels_.data(), options_.width, options_.height,
                         {lastDirty_.x, lastDirty_.y, lastDirty_.w, lastDirty_.h});
    return true;
}

} // namespace spux::inline v1::ui
