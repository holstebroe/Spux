// The Classic flavor's renderer: a supersampled CPU renderer with shading helpers.
//
// It implements IRenderer's basics (rectangles, rounded rectangles, gradients, lines, circles, clipping,
// text in the pre-rasterized label font) on the three CpuLayers (Layers.hpp) at a supersampling factor,
// 2 by default, and box-filters down when the frame is resolved. Shapes have analytic edge coverage, so
// they come out smooth after the filter. On top of the basics it offers what the Classic painters shade
// with: per-pixel shading of a box or a rounded box, soft shadows, printed lettering with wear, and a
// worn-paint panel that is rendered once per size and copied afterwards.
//
// layers() returns the cache, so Gui skips unchanged widgets and the Editor presents only what changed.
// Text and shading are clipped by pushClip like every other call. The effect extensions without an
// override here (blend modes, emission, bloom) are the plain defaults of IRenderer.
#pragma once

#include <cstdint>
#include <vector>

#include "spux/ui/IRenderer.hpp"
#include "spux/ui/Layers.hpp"
#include "spux/ui/classic/ClassicDraw.hpp"

namespace spux::inline v1::ui::classic {

class ShadedRenderer final : public IRenderer {
public:
    // `supersample` is clamped to 1..4.
    ShadedRenderer(int width, int height, int supersample = 2);

    [[nodiscard]] int width() const override { return layers_.width(); }
    [[nodiscard]] int height() const override { return layers_.height(); }
    [[nodiscard]] LayerCache* layers() override { return &layers_; }
    [[nodiscard]] CpuLayers& cpu() { return layers_; }
    [[nodiscard]] int supersample() const { return layers_.scale(); }

    // The finished frame (width x height, opaque ARGB): the whole frame, whatever is dirty.
    [[nodiscard]] std::vector<std::uint32_t> frame() const;

    // --- IRenderer basics -------------------------------------------------------------------------------
    void clear(Argb c) override;
    void pushClip(const Rect& r) override;
    void popClip() override;
    void fillRect(const Rect& r, Argb c) override;
    void strokeRect(const Rect& r, Argb c, float thickness = 1.0f) override;
    void fillRoundRect(const Rect& r, float radius, Argb c) override;
    void fillGradientRect(const Rect& r, Argb top, Argb bottom) override;
    void strokeRoundRect(const Rect& r, float radius, Argb c, float thickness = 1.0f) override;
    void drawLine(Point a, Point b, Argb c, float thickness = 1.0f) override;
    void drawPolyline(const Point* pts, int count, Argb c, float thickness = 1.0f) override;
    void fillCircle(Point center, float radius, Argb c) override;
    void strokeCircle(Point center, float radius, Argb c, float thickness = 1.0f) override;
    // The label font. `scale` multiplies a cap height of 6 logical pixels; (x, y) is the left edge and the top
    // of the capitals.
    void drawText(const char* text, int x, int y, Argb c, int scale = 1) override;
    [[nodiscard]] int textWidth(const char* text, int scale = 1) const override;
    [[nodiscard]] int textHeight(int scale = 1) const override;

    // --- effect extensions with a better-than-plain version ---------------------------------------------
    void drawGlow(Point center, float radius, Argb c) override;
    void fillRadialGradient(Point center, float radius, Argb inner, Argb outer) override;
    void drawShadow(const Rect& r, float radius, float spread, Argb c) override;
    // Text at a pixel size: the capitals are 0.7 of it. Regular is the label font, Bold the display font.
    void drawTextPx(const char* text, float x, float y, Argb c, float px, Align align = Align::Left,
                    FontWeight weight = FontWeight::Regular) override;
    [[nodiscard]] float textWidthPx(const char* text, float px, FontWeight weight = FontWeight::Regular) const override;

    // --- shading helpers ------------------------------------------------------------------------------------
    // The pre-rasterized fonts (one copy in the program, shared by every renderer).
    [[nodiscard]] static const AtlasFont& labelFont();
    [[nodiscard]] static const AtlasFont& displayFont();

    // A view of the current target layer in buffer pixels (supersample() per logical pixel), limited to the
    // current clip. The ClassicDraw helpers (shadeBox, drawLineAA, shadeRoundRect, softShadow, shadeLit ...)
    // draw through it. Valid until the target layer, a clip or the size changes.
    [[nodiscard]] Graphics graphics();

    // Printed lettering: capitals `cap` logical pixels tall, the left edge at x or centered on x; `wear`
    // (0..1) makes the ink patchy like old silk-screen print.
    void ink(const AtlasFont& font, const char* text, float x, float capTop, float cap, Argb c, float wear = 0.f);
    void inkCentered(const AtlasFont& font, const char* text, float centerX, float capTop, float cap, Argb c,
                     float wear = 0.f);
    [[nodiscard]] static float inkWidth(const AtlasFont& font, const char* text, float cap) {
        return classic::textWidth(font, text, cap);
    }

    // A worn silver panel (paintWornPanel) over `r`. The texture of one size is rendered once and kept, so
    // a redraw of the background copies it; a size or trim change renders it again.
    void wornPanel(const Rect& r, bool trims);
    // Brushed steel (paintBrushedPanel) over `r`, cached and copied the same way. The cache holds one panel:
    // asking for the other kind, size or finish renders it again.
    void brushedPanel(const Rect& r, Color light, Color dark, const BrushedSteel& steel);

private:
    std::vector<Rect> clips_;
    CpuLayers layers_;
    // the cached panel texture: what it is (worn or brushed), size in logical pixels, supersampling
    struct PanelKey {
        int kind = 0; // 0 none, 1 worn, 2 brushed
        int w = 0, h = 0, scale = 0;
        bool trims = false;
        Color light{}, dark{};
        BrushedSteel steel;
        bool operator==(const PanelKey& o) const {
            return kind == o.kind && w == o.w && h == o.h && scale == o.scale && trims == o.trims &&
                   light.r == o.light.r && light.g == o.light.g && light.b == o.light.b && dark.r == o.dark.r &&
                   dark.g == o.dark.g && dark.b == o.dark.b && steel == o.steel;
        }
    };
    void paintPanelTexture(const Rect& r, const PanelKey& key);
    PanelKey panelKey_;
    std::vector<std::uint32_t> worn_; // the cached texture, either kind
};

// The renderer a Classic painter was given, or null when it draws on something else.
[[nodiscard]] inline ShadedRenderer* asShaded(IRenderer& r) { return dynamic_cast<ShadedRenderer*>(&r); }

} // namespace spux::inline v1::ui::classic
