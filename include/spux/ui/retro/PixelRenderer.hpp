// The Retro flavor's renderer: integer pixels, no anti-aliasing, bitmap text.
//
// It implements IRenderer's basics on the three CpuLayers (Layers.hpp) at scale 1, so Gui skips unchanged widgets
// and the Editor presents only what changed. Coordinates are logical pixels. A point (x, y) lies in the pixel
// (floor x, floor y); a shape covers a pixel when the pixel's center is inside it. Lines are Bresenham lines with
// a square brush (the thickness rounded to whole pixels, at least one), rectangles and strokes are whole pixels,
// a circle of radius r around the center of pixel (cx, cy), that is the point (cx + 0.5, cy + 0.5), spans
// cx - r to cx + r, and its stroke is the pixels of the circle that are not strictly inside the circle `thickness`
// smaller. Colors blend with integer arithmetic; an opaque color is a plain store.
//
// Text is a BitmapFont (BitmapFont.hpp): the label font for Regular, the bold font for Bold (the label font unless
// one is set), glyphs repeated `scale` times. (x, y) is the left edge and the top of the capitals, as on the other
// renderers.
//
// The effect extensions are the pixel-art version of each: arcs are stamped pixel by pixel, glows, radial gradients
// and shadows are dithered with a 4 x 4 ordered pattern instead of smoothed, the glass panel is a bevel, the level
// bar is a column of segments. Blend modes Alpha and Add are supported; emission is ignored.
#pragma once

#include <cstdint>
#include <vector>

#include "spux/ui/IRenderer.hpp"
#include "spux/ui/Layers.hpp"
#include "spux/ui/retro/BitmapFont.hpp"

namespace spux::inline v1::ui::retro {

class PixelRenderer final : public IRenderer {
public:
    // The label font is the thin 5x7 face. Use the other constructor to bring another font and not link this one.
    PixelRenderer(int width, int height);
    // `bold` is what FontWeight::Bold draws; null: the same as `font`. The fonts must outlive the renderer.
    PixelRenderer(int width, int height, const BitmapFont& font, const BitmapFont* bold = nullptr);

    [[nodiscard]] int width() const override { return layers_.width(); }
    [[nodiscard]] int height() const override { return layers_.height(); }
    [[nodiscard]] LayerCache* layers() override { return &layers_; }
    [[nodiscard]] CpuLayers& cpu() { return layers_; }

    // The finished frame (width x height, opaque ARGB): the whole frame, whatever is dirty.
    [[nodiscard]] std::vector<std::uint32_t> frame() const;

    [[nodiscard]] const BitmapFont& font() const { return *font_; }
    [[nodiscard]] const BitmapFont& boldFont() const { return *bold_; }

    // --- IRenderer basics -------------------------------------------------------------------------------
    void clear(Color c) override;
    void pushClip(const Rect& r) override;
    void popClip() override;
    void fillRect(const Rect& r, Color c) override;
    void strokeRect(const Rect& r, Color c, float thickness = 1.0f) override;
    // The radius is rounded to whole pixels; one pixel cuts nothing, two take the corner pixel off.
    void fillRoundRect(const Rect& r, float radius, Color c) override;
    void strokeRoundRect(const Rect& r, float radius, Color c, float thickness = 1.0f) override;
    void fillGradientRect(const Rect& r, Color top, Color bottom) override;
    void drawLine(Point a, Point b, Color c, float thickness = 1.0f) override;
    void drawPolyline(const Point* pts, int count, Color c, float thickness = 1.0f) override;
    void fillCircle(Point center, float radius, Color c) override;
    void strokeCircle(Point center, float radius, Color c, float thickness = 1.0f) override;
    void drawText(const char* text, int x, int y, Color c, int scale = 1) override;
    [[nodiscard]] int textWidth(const char* text, int scale = 1) const override {
        return font_->textWidth(text, scale);
    }
    // The height of the capitals.
    [[nodiscard]] int textHeight(int scale = 1) const override { return font_->capHeight * scale; }

    // --- effect extensions, pixel-art versions ----------------------------------------------------------
    void setBlend(Blend b) override { blend_ = b; }
    void drawArc(Point center, float radius, float a0, float a1, Color c, float thickness) override;
    void drawGlow(Point center, float radius, Color c) override;
    void fillRadialGradient(Point center, float radius, Color inner, Color outer) override;
    void drawShadow(const Rect& r, float radius, float spread, Color c) override;
    void drawPanel(const Rect& r, float radius, Color tint) override;
    void drawTextPx(const char* text, float x, float y, Color c, float px, Align align = Align::Left,
                    FontWeight weight = FontWeight::Regular) override;
    [[nodiscard]] float textWidthPx(const char* text, float px, FontWeight weight = FontWeight::Regular) const override;
    void drawMeter(const Rect& area, float level, Color c) override;

    // --- pixel helpers for painters ---------------------------------------------------------------------
    // One pixel, clipped and blended.
    void pixel(int x, int y, Color c);
    void hLine(int x, int y, int length, Color c) { fillRect({x, y, length, 1}, c); }
    void vLine(int x, int y, int length, Color c) { fillRect({x, y, 1, length}, c); }
    // A raised or sunken edge: `light` along the top and left, `dark` along the bottom and right (swapped when
    // sunken); the corners belong to the dark edge.
    void bevel(const Rect& r, Color light, Color dark, bool sunken = false);
    // `text` in the given font at `scale`, (x, capTop) the left edge and the top of the capitals.
    void text(const BitmapFont& font, const char* text, int x, int capTop, Color c, int scale = 1);
    // Centered on `centerX`.
    void textCentered(const BitmapFont& font, const char* text, int centerX, int capTop, Color c, int scale = 1);
    // A one-bit picture: `h` row bytes, leftmost column in bit 7, `w` at most 8 columns, set bits drawn.
    void bits(int x, int y, const std::uint8_t* rows, int w, int h, Color c, int scale = 1);
    // The pixels of `r` where a 4 x 4 ordered pattern says so for `level` (0..16 sixteenths of coverage).
    void fillDither(const Rect& r, Color c, int level);

    // The pattern's decision for pixel (x, y) at `level` (0..16): true when the pixel is drawn.
    [[nodiscard]] static bool dither(int x, int y, int level) {
        static constexpr std::uint8_t kBayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
        return level > kBayer[y & 3][x & 3];
    }

private:
    [[nodiscard]] Rect clip() const { return clips_.empty() ? Rect{0, 0, width(), height()} : clips_.back(); }
    void blendAt(std::uint32_t* p, Color c) const;
    void fillSpan(int x0, int x1, int y, Color c, const Rect& clip);
    void stamp(int x, int y, int brush, Color c);
    void line(int x0, int y0, int x1, int y1, int brush, Color c);

    std::vector<Rect> clips_;
    CpuLayers layers_;
    const BitmapFont* font_;
    const BitmapFont* bold_;
    Blend blend_ = Blend::Alpha;
};

// The renderer a Retro painter was given, or null when it draws on something else.
[[nodiscard]] inline PixelRenderer* asPixel(IRenderer& r) { return dynamic_cast<PixelRenderer*>(&r); }

} // namespace spux::inline v1::ui::retro
