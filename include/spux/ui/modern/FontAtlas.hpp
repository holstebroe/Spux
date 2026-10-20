// Signed-distance-field font atlas for the Modern renderers: the embedded Inter instances are rasterized
// once with stb_truetype into one single-channel atlas, when the editor opens. A glyph is drawn at any
// size by thresholding the distance field (in the GPU shader, or on the CPU by the software renderer).
// Digits share one advance (tabular numerals), so values that change do not make their neighbors jump.
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "spux/ui/Geometry.hpp"

namespace spux::inline v1::ui::modern {

class FontAtlas {
public:
    static constexpr int kSize = 1024;      // atlas side in texels
    static constexpr float kBakePx = 32.0f; // size the glyphs are rasterized at
    static constexpr int kSpread = 5;       // distance field half-width in texels
    static constexpr int kOnEdge = 128;     // texel value on the glyph outline

    struct Glyph {
        float u0 = 0, v0 = 0, u1 = 0, v1 = 0; // atlas coordinates
        // quad in em units (1 = the font size), relative to the pen, y down from the baseline
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        float advance = 0; // em units
        bool valid = false;
    };

    // Rasterizes both weights; false when the font data is unusable.
    bool build();
    [[nodiscard]] const std::vector<std::uint8_t>& pixels() const { return pixels_; }

    // The glyph for a Unicode code point (a space or an unknown code point has no quad but an advance).
    [[nodiscard]] const Glyph& glyph(FontWeight w, std::uint32_t cp) const;
    // Width of a UTF-8 string in em units.
    [[nodiscard]] float width(FontWeight w, std::string_view utf8) const;

    static constexpr float kCapHeight = 0.727f; // Inter: the top of capital letters above the baseline, em units
    static constexpr float kLineHeight = 1.21f;

    // Next code point of a UTF-8 string; advances `i`. Invalid bytes become '?'.
    static std::uint32_t nextCodePoint(std::string_view s, std::size_t& i);

private:
    static constexpr std::uint32_t kFirst = 32, kLast = 0x25FF;
    std::vector<std::uint8_t> pixels_;
    std::vector<Glyph> glyphs_[2]; // indexed by slot, for the code points baked
    std::vector<std::uint32_t> codepoints_;
    Glyph none_;
    [[nodiscard]] int index(std::uint32_t cp) const;
    std::vector<std::int32_t> map_; // code point -> slot, -1 none
};

} // namespace spux::inline v1::ui::modern
