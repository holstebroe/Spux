#ifndef SPUX_UI_CLASSIC_GRAPHICS_HPP
#define SPUX_UI_CLASSIC_GRAPHICS_HPP

#include "spux/ui/classic/Font.hpp"
#include <cstddef>
#include <cstdint>

namespace spux::inline v1::ui::classic {

class Graphics {
public:
    Graphics(uint32_t* buffer, uint32_t width, uint32_t height, int scale = 1);
    ~Graphics() = default;

    void clear(uint32_t color);
    void setPixel(int x, int y, uint32_t color);

    void drawLine(int x0, int y0, int x1, int y1, uint32_t color, int thickness = 1);
    void drawRect(int x, int y, int width, int height, uint32_t color);
    void fillRect(int x, int y, int width, int height, uint32_t color);

    void drawCircle(int cx, int cy, int radius, uint32_t color, int thickness = 1);
    void fillCircle(int cx, int cy, int radius, uint32_t color);

    void drawText(const Font& font, const char* text, int x, int y, uint32_t color, int textScale = 1);

    // Blend one pixel in buffer coordinates (logical x scale), no scaling.
    void blendPixel(int bx, int by, uint32_t srcColor);

    // Supersampling factor and raw buffer, for skins that shade per buffer
    // pixel (see blendPixel).
    int getScale() const { return scale_; }
    uint32_t* getBuffer() { return buffer_; }

    // Limits blendPixel (and so every drawing call) to [bx0, bx1) x [by0, by1) in buffer coordinates; the
    // default is the whole buffer. The box is clamped to the buffer.
    void setClip(int bx0, int by0, int bx1, int by1);
    void clearClip() { setClip(0, 0, static_cast<int>(bufferWidth_), static_cast<int>(bufferHeight_)); }
    int clipX0() const { return clipX0_; }
    int clipY0() const { return clipY0_; }
    int clipX1() const { return clipX1_; }
    int clipY1() const { return clipY1_; }

    uint32_t getWidth() const { return logicalWidth_; }
    uint32_t getHeight() const { return logicalHeight_; }

private:
    uint32_t* buffer_{nullptr};
    uint32_t logicalWidth_{0};
    uint32_t logicalHeight_{0};
    int scale_{1};
    uint32_t bufferWidth_{0};
    uint32_t bufferHeight_{0};
    int clipX0_{0}, clipY0_{0}, clipX1_{0}, clipY1_{0};
};

} // namespace spux::inline v1::ui::classic

#endif // SPUX_UI_CLASSIC_GRAPHICS_HPP
