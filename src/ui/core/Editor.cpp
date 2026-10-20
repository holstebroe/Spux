#include "spux/ui/Editor.hpp"

#include <algorithm>

namespace spux::inline v1::ui {

EditorCore::EditorCore(EditorHost& host, GuiCore& gui, RootViewBase& root, const EditorOptions& options,
                       RendererFactory renderer, SurfaceFactory surface)
    : host_(host), gui_(gui), root_(root), options_(options), makeRenderer_(std::move(renderer)),
      makeSurface_(std::move(surface)) {
    options_.width = std::max(1, options_.width);
    options_.height = std::max(1, options_.height);
    options_.minScale = std::max(0.1f, options_.minScale);
    options_.maxScale = std::max(options_.minScale, options_.maxScale);
    userScale_ = std::clamp(options_.scale, options_.minScale, options_.maxScale);
    pixels_.assign(static_cast<std::size_t>(options_.width) * static_cast<std::size_t>(options_.height), 0xFF000000u);
    start_ = std::chrono::steady_clock::now();
}

EditorCore::~EditorCore() { destroy(); }

double EditorCore::clockNow() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
}

// --- window and scale ---------------------------------------------------------------------------------------

int EditorCore::physical(int logical) const {
    return static_cast<int>(std::lround(static_cast<float>(logical) * scaleLocked()));
}

bool EditorCore::openWindow(const platform::ParentWindow& parent, bool gl, float scale) {
    auto w = platform::NativeWindow::create();
    platform::WindowOptions wo;
    wo.keyboard = options_.keyboard;
    wo.openGl = gl;
    const int pw = static_cast<int>(std::lround(static_cast<float>(options_.width) * scale));
    const int ph = static_cast<int>(std::lround(static_cast<float>(options_.height) * scale));
    if (!w || !w->open(parent, pw, ph, this, wo))
        return false;
    window_ = std::move(w);
    return true;
}

bool EditorCore::setParent(const platform::ParentWindow& parent) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (window_ || !platform::NativeWindow::supports(parent.api))
        return false;
    // the GPU first: a window with a GL pixel format and a surface bound to it; anything failing means the
    // CPU renderer in a plain window
    if (makeSurface_) {
        std::unique_ptr<FrameSurface> surface = makeSurface_();
        bool attached = false;
        if (surface && openWindow(parent, true, userScale_)) {
            const float system = std::max(1.0f, window_->contextScale());
            std::string why;
            attached = surface->attach(*window_, options_.width, options_.height, userScale_ * system, &why);
            if (attached) {
                systemScale_ = system;
                surface_ = std::move(surface);
                if (system != 1.0f)
                    window_->setSize(physical(options_.width), physical(options_.height));
            }
        }
        if (!attached) {
            if (surface)
                surface->detach();
            surface.reset();
            if (window_) {
                // closing joins the window's event thread, whose callbacks take the lock: let go of it meanwhile
                std::unique_ptr<platform::NativeWindow> dead = std::move(window_);
                lock.unlock();
                dead->close();
                lock.lock();
            }
        }
    }
    if (!surface_ && !openWindow(parent, false, 1.0f))
        return false;
    tickHz_ = options_.idleHz;
    window_->setTickRate(tickHz_);
    first_ = true;
    return true;
}

Rect EditorCore::lastDirty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastDirty_;
}

int EditorCore::width() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return physical(options_.width);
}
int EditorCore::height() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return physical(options_.height);
}

bool EditorCore::canResize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return surface_ != nullptr;
}

void EditorCore::adjustSize(int& w, int& h) const {
    std::lock_guard<std::mutex> lock(mutex_);
    float s = 1.0f;
    if (surface_)
        s = std::clamp(static_cast<float>(w) / static_cast<float>(options_.width) / systemScale_, options_.minScale,
                       options_.maxScale) *
            systemScale_;
    w = static_cast<int>(std::lround(static_cast<float>(options_.width) * s));
    h = static_cast<int>(std::lround(static_cast<float>(options_.height) * s));
}

void EditorCore::applyScale() {
    scaleDirty_ = true;
    forcePending_.store(true, std::memory_order_relaxed); // an idle editor still redraws at the new size
    if (window_)
        window_->setSize(physical(options_.width), physical(options_.height));
}

bool EditorCore::setSize(int width, int height) {
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192)
        return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!surface_)
        return width == options_.width && height == options_.height;
    userScale_ = std::clamp(static_cast<float>(width) / static_cast<float>(options_.width) / systemScale_,
                            options_.minScale, options_.maxScale);
    applyScale();
    return true;
}

bool EditorCore::setScale(double systemScale) {
    if (!(systemScale >= 0.5 && systemScale <= 8.0))
        return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!surface_)
        return false;
    systemScale_ = static_cast<float>(systemScale);
    applyScale();
    return true;
}

bool EditorCore::setUserScale(float scale) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!surface_ || !(scale > 0.0f))
        return false;
    const float s = std::clamp(scale, options_.minScale, options_.maxScale);
    if (s != userScale_) {
        userScale_ = s;
        applyScale();
    }
    return true;
}

float EditorCore::userScale() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return userScale_;
}
float EditorCore::scale() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return scaleLocked();
}
bool EditorCore::usesGpu() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return surface_ != nullptr;
}
const char* EditorCore::rendererName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return surface_ ? surface_->name() : "software";
}
int EditorCore::degradation() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return surface_ ? surface_->degradation() : 0;
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
        // GPU objects die while the window still exists (the driver asks the window about its size); a tick
        // that comes later finds no surface and draws nothing
        if (surface_)
            surface_->detach();
        surface_.reset();
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

// The window reports pixels; the page works in logical pixels.
void EditorCore::onMouseDown(int x, int y, MouseButton b, bool shift) {
    const float s = scale();
    mouseDown(static_cast<float>(x) / s, static_cast<float>(y) / s, b, shift);
}
void EditorCore::onMouseUp(int x, int y, MouseButton b) {
    const float s = scale();
    mouseUp(static_cast<float>(x) / s, static_cast<float>(y) / s, b);
}
void EditorCore::onMouseMove(int x, int y, bool, bool shift) {
    const float s = scale();
    mouseMove(static_cast<float>(x) / s, static_cast<float>(y) / s, shift);
}
void EditorCore::onMouseWheel(int x, int y, float steps, bool shift) {
    const float s = scale();
    mouseWheel(static_cast<float>(x) / s, static_cast<float>(y) / s, steps, shift);
}
void EditorCore::onMouseLeave() { mouseLeave(); }
void EditorCore::onKey(const KeyEvent& k) { key(k); }

// --- frames -------------------------------------------------------------------------------------------------

bool EditorCore::frame(bool force, double now) {
    // a tick never waits for the host's thread: it skips the frame while the editor is being changed
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
        if (force)
            forcePending_.store(true, std::memory_order_relaxed); // an expose must not be lost
        return false;
    }
    return frameLocked(force, now < 0.0 ? clockNow() : now);
}

bool EditorCore::frameLocked(bool force, double now) {
    if (window_ && !visible_)
        return false;
    if (!surface_ && !renderer_) {
        renderer_ = makeRenderer_ ? makeRenderer_(options_.width, options_.height) : nullptr;
        if (!renderer_)
            return false;
        first_ = true;
    }
    IRenderer* renderer = surface_ ? surface_->renderer() : renderer_.get();
    if (!renderer)
        return false;
    root_.tick(now);
    const std::uint32_t params = host_.paramVersion();
    const std::uint64_t content = root_.contentVersion();
    const std::uint64_t bgKey = root_.backgroundKey();
    const bool animating = root_.animating();
    const bool needed = force || forcePending_.exchange(false, std::memory_order_relaxed) || first_ || input_ ||
                        params != seenParams_ || content != seenContent_ || bgKey != seenBackground_ || animating ||
                        gui_.wantsFrame(now);

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

    if (surface_) {
        if (scaleDirty_) {
            if (!surface_->resize(options_.width, options_.height, scaleLocked())) {
                forcePending_.store(true, std::memory_order_relaxed); // keep scaleDirty_: try again next tick
                return false;
            }
            scaleDirty_ = false;
            first_ = true;
        }
        if (!surface_->beginFrame(now)) {
            forcePending_.store(true, std::memory_order_relaxed); // the context is not ready: try again
            return false;
        }
    }
    IRenderer& r = *renderer;
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
    if (surface_) {
        surface_->endFrame(); // the surface presents; the whole frame is new
        lastDirty_ = {0, 0, options_.width, options_.height};
    } else if (layers) {
        lastDirty_ = layers->dirtyBounds();
        if (first_)
            lastDirty_ = {0, 0, options_.width, options_.height};
        layers->resolve(pixels_.data());
        layers->clearDirty();
    } else {
        lastDirty_ = {0, 0, options_.width, options_.height};
    }
    first_ = false;
    framesDrawn_.fetch_add(1, std::memory_order_relaxed);
    if (window_ && !surface_ && !lastDirty_.empty())
        window_->present(pixels_.data(), options_.width, options_.height,
                         {lastDirty_.x, lastDirty_.y, lastDirty_.w, lastDirty_.h});
    return true;
}

} // namespace spux::inline v1::ui
