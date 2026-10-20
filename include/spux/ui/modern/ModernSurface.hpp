// The Modern flavor's way onto the screen: a GL 3.3 context on the editor's window, the GpuRenderer, and a
// quality governor (FrameSurface.hpp). With modernBackend() an Editor<ModernStyle> tries the GPU and falls
// back to the SoftRenderer on its own:
//
//   auto backend = spux::ui::modern::modernBackend();
//   spux::ui::Editor<spux::ui::modern::ModernStyle> editor(host, style, root, options, backend.software,
//                                                          backend.gpu);
//
// The GPU is not used when the context is older than 3.3, when OpenGL cannot be loaded, or (in Auto) when
// the driver is a software rasterizer: the CPU renderer is faster than one. Choice::Gpu uses it anyway.
// The environment variable SPUX_RENDERER=gpu|soft overrides the choice, for development.
#pragma once

#include <memory>
#include <string>

#include "spux/gl/GlContext.hpp"
#include "spux/ui/Editor.hpp"
#include "spux/ui/modern/GpuRenderer.hpp"

namespace spux::inline v1::ui::modern {

enum class RendererChoice { Auto, Gpu, Software };

struct SurfaceOptions {
    RendererChoice choice = RendererChoice::Auto;
    GpuRenderer::Look look;
    bool reducedMotion = false; // no film grain, a still backdrop
    float budgetMs = 6.0f;      // GPU time per frame the governor aims for
};

// Lowers the effects when the GPU is over its frame budget and raises them again when it has room.
// Levels: 0 full, 1 fewer bloom levels, 2 bloom from a smaller target, 3 one bloom level, 4 no bloom.
class QualityGovernor {
public:
    static constexpr int kMaxLevel = 4;
    explicit QualityGovernor(float budgetMs = 6.0f) : budget_(budgetMs) {}
    // One frame's GPU time in milliseconds; 0 (no timer queries) changes nothing.
    void update(float gpuMs);
    [[nodiscard]] int level() const { return level_; }
    void reset() { level_ = slow_ = fast_ = 0, avg_ = 0.0f; }

private:
    float budget_;
    int level_ = 0, slow_ = 0, fast_ = 0;
    float avg_ = 0.0f;
};

// The renderer settings for a governor level.
GpuRenderer::Quality qualityForLevel(int level, bool reducedMotion = false);

class ModernSurface final : public FrameSurface {
public:
    explicit ModernSurface(const SurfaceOptions& options = {});
    ~ModernSurface() override;

    bool attach(platform::NativeWindow& window, int logicalWidth, int logicalHeight, float scale,
                std::string* why) override;
    bool resize(int logicalWidth, int logicalHeight, float scale) override;
    [[nodiscard]] IRenderer* renderer() override { return gpu_.get(); }
    bool beginFrame(double now) override;
    void endFrame() override;
    void detach() override;
    [[nodiscard]] const char* name() const override { return name_.c_str(); }
    [[nodiscard]] int degradation() const override { return governor_.level(); }

    // The renderer, for settings between frames (GpuRenderer::setLook); null before attach.
    [[nodiscard]] GpuRenderer* gpu() { return gpu_.get(); }

private:
    SurfaceOptions options_;
    QualityGovernor governor_;
    std::unique_ptr<gl::GlContext> ctx_;
    gl::GlApi api_;
    std::unique_ptr<GpuRenderer> gpu_; // after api_: it holds a reference to it
    std::string name_ = "none";
};

// What Editor<Style> needs for the Modern flavor: the surface factory to try first (empty when the choice
// is Software) and the factory of the CPU renderer it falls back to (supersample 1 to 4).
struct ModernBackend {
    SurfaceFactory gpu;
    RendererFactory software;
};
ModernBackend modernBackend(const SurfaceOptions& options = {}, int supersample = 2);

} // namespace spux::inline v1::ui::modern
