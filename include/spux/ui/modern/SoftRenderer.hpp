// Software renderer for the Modern style: the same drawing calls as the GPU renderer with the effects
// drawn plain (no bloom, no shadows, no emission), so a page lays out and reads the same on machines
// without a usable GPU (remote and virtual sessions, broken drivers) and in headless tests.
//
// Shapes are signed-distance primitives with analytic edge coverage, drawn into the three CpuLayers
// (Layers.hpp) at a supersampling factor and box-filtered down when the frame is resolved. The factor is
// the quality knob: 1 draws at the window's pixels (the analytic edges are still anti-aliased), 2 is the
// default, 3 and 4 cost the square of it in fill time. Text is the same Inter distance-field atlas the GPU
// renderer samples, thresholded per pixel, so glyph shapes and advances match across both renderers.
// Blend::Add accumulates light; the emission level is ignored.
//
// layers() returns the cache, so Gui skips unchanged widgets and the Editor presents only what changed.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "spux/ui/IRenderer.hpp"
#include "spux/ui/Layers.hpp"
#include "spux/ui/modern/FontAtlas.hpp"

namespace spux::inline v1::ui::modern {

class SoftRenderer final : public IRenderer {
public:
    // `supersample` is clamped to 1..4. The font atlas is baked here (about a tenth of a second); pass one
    // from makeFontAtlas() to share it between renderers: it is immutable once built.
    SoftRenderer(int width, int height, int supersample = 2, std::shared_ptr<const FontAtlas> font = nullptr);
    static std::shared_ptr<const FontAtlas> makeFontAtlas();

    [[nodiscard]] int width() const override { return layers_.width(); }
    [[nodiscard]] int height() const override { return layers_.height(); }
    [[nodiscard]] LayerCache* layers() override { return &layers_; }
    [[nodiscard]] CpuLayers& cpu() { return layers_; }
    [[nodiscard]] int supersample() const { return layers_.scale(); }

    // The finished frame (width x height, opaque ARGB): the whole frame, whatever is dirty.
    [[nodiscard]] std::vector<std::uint32_t> frame() const;

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
    void setBlend(Blend b) override { blend_ = b; }
    void setEmission(float) override {}
    void drawArc(Point center, float radius, float a0, float a1, Color c, float thickness) override;
    void drawGlow(Point center, float radius, Color c) override;
    void fillRadialGradient(Point center, float radius, Color inner, Color outer) override;
    void drawPanel(const Rect& r, float radius, Color tint) override;
    void drawStrip(const Point* pts, const Color* colors, const float* widths, int count) override;
    void fillUnder(const Point* pts, int count, float baselineY, Color top, Color bottom) override;
    void drawTextPx(const char* text, float x, float y, Color c, float px, Align align = Align::Left,
                    FontWeight weight = FontWeight::Regular) override;
    [[nodiscard]] float textWidthPx(const char* text, float px, FontWeight weight = FontWeight::Regular) const override;

private:
    struct Hi { // a rectangle in supersampled pixels, [x0, x1) x [y0, y1)
        int x0, y0, x1, y1;
    };
    [[nodiscard]] Hi clipHi() const;
    [[nodiscard]] Hi boxHi(float x0, float y0, float x1, float y1) const; // logical floats, clipped
    void put(int hx, int hy, Color c, float coverage);
    void box(const Rect& r, float radius, float stroke, Color top, Color bottom);
    void strip(const Point* pts, const Color* colors, const float* widths, int count);

    CpuLayers layers_;
    std::vector<Rect> clips_;
    Blend blend_ = Blend::Alpha;
    std::shared_ptr<const FontAtlas> font_;
    std::vector<float> maskCover_;         // scratch for strips: best coverage per pixel
    std::vector<std::uint32_t> maskColor_; // and the color of the segment that gave it
};

} // namespace spux::inline v1::ui::modern
