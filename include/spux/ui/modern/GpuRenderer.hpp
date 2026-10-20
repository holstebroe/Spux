// GPU renderer: implements IRenderer on OpenGL 3.3 core. Everything is drawn as batched quads with
// signed-distance shapes in one shader (rounded boxes, circles, capsules, arcs, glows, SDF text, polyline
// strips) into a linear RGBA16F scene target; a dual-filter bloom chain and a composite pass (tone map,
// vignette, grain, dither, sRGB) put it on screen. The renderer owns every GPU object it uses, creates
// them in create() and frees them in the destructor with the context current. Nothing is process-global,
// so several editors in one host do not interfere.
//
// Frame: beginFrame, drawBackground (optional), the drawing calls, endFrame; the caller swaps buffers.
// Emission above 1 makes a color glow through the bloom (setEmission); Blend::Add accumulates light.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "spux/gl/GlApi.hpp"
#include "spux/ui/IRenderer.hpp"
#include "spux/ui/modern/FontAtlas.hpp"

namespace spux::inline v1::ui::modern {

class GpuRenderer final : public IRenderer {
public:
    // What a frame costs; a quality governor lowers these when the GPU is over budget.
    struct Quality {
        bool bloom = true;
        int bloomLevels = 5;         // 1..5 levels of the dual-filter chain
        float bloomStrength = 1.0f;  // times Look::bloomStrength
        bool bloomDownscale = false; // the chain starts at a quarter of the resolution instead of a half
        bool grain = true;
        bool animatedBackground = true;
    };

    // How the post chain and the backdrop look; the numbers are the renderer's own defaults.
    struct Look {
        float bloomThreshold = 0.7f;  // brightness above which a pixel feeds the bloom
        float bloomStrength = 0.55f;  // bloom added to the scene
        float grain = 0.012f;         // amplitude of the film grain (Quality::grain switches it on)
        float vignette = 0.55f;       // darkening towards the corners
        float backgroundPools = 1.0f; // strength of the two accent-colored pools of light in the backdrop
        float backgroundDust = 1.0f;  // strength of the faint twinkling points in the backdrop
    };

    // The context must be current. Returns nullptr and a reason when shaders fail to compile or a target
    // cannot be made (the editor then uses the software renderer). `scale` is framebuffer pixels per
    // logical pixel; `offscreen` renders into a texture that readPixels returns instead of the window.
    static std::unique_ptr<GpuRenderer> create(const gl::GlApi& api, int logicalWidth, int logicalHeight, float scale,
                                               bool offscreen, std::string* error);
    ~GpuRenderer() override;

    // Logical size and pixels per logical pixel; re-creates the targets when they change.
    bool resize(int logicalWidth, int logicalHeight, float scale);
    void setQuality(const Quality& q) { quality_ = q; }
    [[nodiscard]] const Quality& quality() const { return quality_; }
    void setLook(const Look& l) { look_ = l; }
    [[nodiscard]] const Look& look() const { return look_; }

    void beginFrame(double timeSeconds);
    void endFrame();
    // Offscreen mode only (after endFrame): the frame as 0xFFRRGGBB, top row first.
    bool readPixels(std::vector<std::uint32_t>& out);
    // GPU time of a recent frame in milliseconds (timer queries, a frame or two late); 0 when unsupported.
    [[nodiscard]] float gpuMs() const { return gpuMs_; }
    [[nodiscard]] int drawCalls() const { return drawCalls_; }
    [[nodiscard]] int vertexCount() const { return vertexCountFrame_; }

    // --- IRenderer ---
    [[nodiscard]] int width() const override { return w_; }
    [[nodiscard]] int height() const override { return h_; }
    [[nodiscard]] bool isGpu() const override { return true; }
    void clear(Color c) override;
    void pushClip(const Rect& r) override;
    void popClip() override;
    void fillRect(const Rect& r, Color c) override;
    void strokeRect(const Rect& r, Color c, float thickness = 1.0f) override;
    void fillRoundRect(const Rect& r, float radius, Color c) override;
    void fillGradientRect(const Rect& r, Color top, Color bottom) override;
    void strokeRoundRect(const Rect& r, float radius, Color c, float thickness = 1.0f) override;
    void drawLine(Point a, Point b, Color c, float thickness = 1.0f) override;
    void drawPolyline(const Point* pts, int count, Color c, float thickness = 1.0f) override;
    void fillCircle(Point center, float radius, Color c) override;
    void strokeCircle(Point center, float radius, Color c, float thickness = 1.0f) override;
    void drawText(const char* text, int x, int y, Color c, int scale = 1) override;
    [[nodiscard]] int textWidth(const char* text, int scale = 1) const override;
    [[nodiscard]] int textHeight(int scale = 1) const override;
    void drawTrace(const float* values, int n, const Rect& area, float lo, float hi, Color c) override;
    void drawMeter(const Rect& area, float level, Color c) override;
    void drawBackground(Color a, Color b, float time) override;
    void setBlend(Blend b) override;
    void setEmission(float e) override { emission_ = e; }
    void drawArc(Point center, float radius, float a0, float a1, Color c, float thickness) override;
    void drawGlow(Point center, float radius, Color c) override;
    void fillRadialGradient(Point center, float radius, Color inner, Color outer) override;
    void drawShadow(const Rect& r, float radius, float blur, Color c) override;
    void drawPanel(const Rect& r, float radius, Color tint) override;
    void drawStrip(const Point* pts, const Color* colors, const float* widths, int count) override;
    void fillUnder(const Point* pts, int count, float baselineY, Color top, Color bottom) override;
    void drawTextPx(const char* text, float x, float y, Color c, float px, Align align = Align::Left,
                    FontWeight weight = FontWeight::Regular) override;
    [[nodiscard]] float textWidthPx(const char* text, float px, FontWeight weight = FontWeight::Regular) const override;

private:
    explicit GpuRenderer(const gl::GlApi& api) : gl_(api) {}

    struct Vertex {
        float x, y;   // framebuffer pixels
        float px, py; // local coordinates or atlas uv
        float shape[4];
        std::uint8_t c0[4];
        std::uint8_t c1[4];
        float kind, emit;
    };
    struct Target {
        gl::GLuint tex = 0, fbo = 0;
        int w = 0, h = 0;
    };
    struct Program {
        gl::GLuint id = 0;
        gl::GLint res = -1, tex = -1, texel = -1, time = -1, threshold = -1, weight = -1, colA = -1, colB = -1,
                  scene = -1, bloom = -1, bloomStrength = -1, grain = -1, vignette = -1, pools = -1, dust = -1;
    };
    enum Kind {
        kBox = 0,
        kCircle = 1,
        kCapsule = 2,
        kArc = 3,
        kGlow = 4,
        kRadial = 5,
        kText = 6,
        kShadow = 7,
        kStrip = 8,
        kSolid = 9
    };

    bool init(bool offscreen, std::string* error);
    bool buildProgram(Program& p, const char* vert, const char* frag, std::string* error);
    bool makeTarget(Target& t, int w, int h, gl::GLenum internalFormat, gl::GLenum format, gl::GLenum type);
    void destroyTarget(Target& t);
    bool createTargets();
    void destroyTargets();

    void flush();
    void applyScissor();
    void bindTarget(const Target* t, int w, int h);
    void drawFullscreen();

    struct Corner {
        float x, y, px, py;
        Color c0;
    };
    void emitQuad(const Corner (&c)[4], const float (&shape)[4], Color c1, Kind kind);
    void emitVertex(float x, float y, float px, float py, const float (&shape)[4], Color c0, Color c1, Kind kind);
    void box(float x, float y, float w, float h, float radius, float stroke, Color top, Color bottom);

    const gl::GlApi& gl_;
    Quality quality_;
    Look look_;
    bool offscreen_ = false;
    int w_ = 0, h_ = 0, fbW_ = 0, fbH_ = 0;
    float scale_ = 1.0f;
    double time_ = 0.0;

    Program ui_, bg_, bright_, down_, up_, composite_;
    gl::GLuint vao_ = 0, vbo_ = 0, atlasTex_ = 0;
    Target scene_, out_;
    static constexpr int kMaxBloom = 5;
    Target bloom_[kMaxBloom];
    FontAtlas font_;

    std::vector<Vertex> verts_;
    Blend blend_ = Blend::Alpha;
    float emission_ = 1.0f;
    std::vector<Rect> clips_;
    bool scissorOn_ = false;
    bool clearedThisFrame_ = false;
    gl::GLuint queries_[2] = {0, 0};
    bool queryPending_[2] = {false, false};
    int queryIndex_ = 0;
    bool queryRunning_ = false;
    float gpuMs_ = 0.0f;
    int drawCalls_ = 0, vertexCountFrame_ = 0;
};

} // namespace spux::inline v1::ui::modern
