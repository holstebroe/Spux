// Acceptance gate (DESIGN_GUIDELINES 7): an IRenderer over spux's CpuLayers that draws with a plugin's
// own `Graphics` class (Acidus and Tannhauser each have a copy of the Classic drawing code), so the
// ported panels draw with exactly the code their retained versions use. Painters reach the plugin's
// drawing through graphics(), which targets the layer Gui has selected. Clipping is not supported
// (Graphics has none); the basics exist for completeness.
#pragma once

#include <cmath>
#include <cstdint>

#include "spux/ui/IRenderer.hpp"
#include "spux/ui/Layers.hpp"

namespace gate {

using namespace spux::ui;

template <class G, class F> class GraphicsRenderer final : public IRenderer {
public:
    GraphicsRenderer(int w, int h, int scale, const F& font) : layers_(w, h, scale), font_(font) {}

    [[nodiscard]] G graphics() {
        return G(layers_.target(), static_cast<std::uint32_t>(layers_.width()),
                 static_cast<std::uint32_t>(layers_.height()), layers_.scale());
    }
    [[nodiscard]] CpuLayers& cpu() { return layers_; }
    [[nodiscard]] const F& font() const { return font_; }

    [[nodiscard]] int width() const override { return layers_.width(); }
    [[nodiscard]] int height() const override { return layers_.height(); }
    LayerCache* layers() override { return &layers_; }

    void clear(Color c) override { graphics().clear(c); }
    void pushClip(const Rect&) override {}
    void popClip() override {}
    void fillRect(const Rect& r, Color c) override { graphics().fillRect(r.x, r.y, r.w, r.h, c); }
    void strokeRect(const Rect& r, Color c, float) override { graphics().drawRect(r.x, r.y, r.w, r.h, c); }
    void fillRoundRect(const Rect& r, float, Color c) override { fillRect(r, c); }
    void fillGradientRect(const Rect& r, Color top, Color) override { fillRect(r, top); }
    void drawLine(Point a, Point b, Color c, float t) override {
        graphics().drawLine(static_cast<int>(a.x), static_cast<int>(a.y), static_cast<int>(b.x), static_cast<int>(b.y),
                            c, static_cast<int>(std::lround(t)));
    }
    void drawPolyline(const Point* pts, int n, Color c, float t) override {
        for (int i = 0; i + 1 < n; ++i)
            drawLine(pts[i], pts[i + 1], c, t);
    }
    void fillCircle(Point p, float r, Color c) override {
        graphics().fillCircle(static_cast<int>(p.x), static_cast<int>(p.y), static_cast<int>(std::lround(r)), c);
    }
    void strokeCircle(Point p, float r, Color c, float t) override {
        graphics().drawCircle(static_cast<int>(p.x), static_cast<int>(p.y), static_cast<int>(std::lround(r)), c,
                              static_cast<int>(std::lround(t)));
    }
    void drawText(const char* text, int x, int y, Color c, int scale) override {
        graphics().drawText(font_, text, x, y, c, scale);
    }
    [[nodiscard]] int textWidth(const char* text, int scale) const override { return font_.getTextWidth(text, scale); }
    [[nodiscard]] int textHeight(int scale) const override { return static_cast<int>(font_.getHeight()) * scale; }

private:
    CpuLayers layers_;
    F font_;
};

} // namespace gate
