#include "spux/ui/modern/ModernSurface.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "spux/ui/modern/SoftRenderer.hpp"

namespace spux::inline v1::ui::modern {

void QualityGovernor::update(float gpuMs) {
    if (gpuMs <= 0.0f)
        return;
    avg_ = avg_ * 0.95f + gpuMs * 0.05f;
    if (avg_ > budget_) {
        ++slow_;
        fast_ = 0;
    } else if (avg_ < budget_ * 0.55f) {
        ++fast_;
        slow_ = 0;
    } else {
        slow_ = fast_ = 0;
    }
    if (slow_ > 45 && level_ < kMaxLevel) {
        ++level_;
        slow_ = 0;
    }
    if (fast_ > 400 && level_ > 0) {
        --level_;
        fast_ = 0;
    }
}

GpuRenderer::Quality qualityForLevel(int level, bool reducedMotion) {
    level = std::clamp(level, 0, QualityGovernor::kMaxLevel);
    GpuRenderer::Quality q;
    q.bloom = level < 4;
    q.bloomLevels = level == 0 ? 5 : level == 1 ? 3 : level == 2 ? 2 : 1;
    q.bloomDownscale = level >= 2;
    q.grain = level < 2 && !reducedMotion;
    q.animatedBackground = level < 4 && !reducedMotion;
    return q;
}

namespace {

// SPUX_RENDERER=gpu or soft overrides what the plugin asked for.
RendererChoice choiceFor(RendererChoice asked) {
    if (const char* e = std::getenv("SPUX_RENDERER")) {
        if (std::strcmp(e, "gpu") == 0)
            return RendererChoice::Gpu;
        if (std::strcmp(e, "soft") == 0)
            return RendererChoice::Software;
    }
    return asked;
}

} // namespace

ModernSurface::ModernSurface(const SurfaceOptions& options) : options_(options), governor_(options.budgetMs) {
    options_.choice = choiceFor(options.choice);
}

ModernSurface::~ModernSurface() { detach(); }

bool ModernSurface::attach(platform::NativeWindow& window, int w, int h, float scale, std::string* why) {
    std::string reason;
    ctx_ = gl::GlContext::createForWindow(window.nativeHandle(), &reason);
    if (!ctx_ || !ctx_->makeCurrent() || !ctx_->loadApi(api_)) {
        if (why)
            *why = reason.empty() ? "no OpenGL context" : reason;
        detach();
        return false;
    }
    if (options_.choice == RendererChoice::Auto && api_.softwareRenderer()) {
        if (why)
            *why = std::string("software OpenGL renderer: ") + api_.renderer;
        ctx_->doneCurrent();
        detach();
        return false;
    }
    gpu_ = GpuRenderer::create(api_, w, h, scale, false, &reason);
    if (!gpu_) {
        if (why)
            *why = reason;
        ctx_->doneCurrent();
        detach();
        return false;
    }
    gpu_->setLook(options_.look);
    ctx_->setSwapInterval(1);
    ctx_->doneCurrent(); // a context is current in one thread at a time: each frame takes it
    name_ = api_.renderer;
    return true;
}

bool ModernSurface::resize(int w, int h, float scale) {
    if (!gpu_ || !ctx_->makeCurrent())
        return false;
    const bool ok = gpu_->resize(w, h, scale);
    ctx_->doneCurrent();
    return ok;
}

bool ModernSurface::beginFrame(double now) {
    if (!gpu_ || !ctx_->makeCurrent())
        return false;
    gpu_->setQuality(qualityForLevel(governor_.level(), options_.reducedMotion));
    gpu_->beginFrame(now);
    return true;
}

void ModernSurface::endFrame() {
    gpu_->endFrame();
    ctx_->swapBuffers();
    ctx_->doneCurrent();
    governor_.update(gpu_->gpuMs());
}

void ModernSurface::detach() {
    // GPU objects die with their context current, and while the window still exists
    if (ctx_ && gpu_) {
        if (ctx_->makeCurrent()) {
            gpu_.reset();
            ctx_->doneCurrent();
        } else {
            // deleting GL objects without their context is undefined: leak them rather than crash the host
            (void)gpu_.release();
        }
    }
    gpu_.reset();
    ctx_.reset();
    name_ = "none";
}

ModernBackend modernBackend(const SurfaceOptions& options, int supersample) {
    ModernBackend b;
    if (choiceFor(options.choice) != RendererChoice::Software)
        b.gpu = [options] { return std::make_unique<ModernSurface>(options); };
    b.software = [supersample](int w, int h) {
        return std::make_unique<SoftRenderer>(w, h, supersample, FontAtlas::shared());
    };
    return b;
}

} // namespace spux::inline v1::ui::modern
