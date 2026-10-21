// Bitmap fonts of the Retro flavor: a table of glyphs, one byte per pixel row with the leftmost column in bit 7
// (so a glyph is at most 8 pixels wide), in ASCII order. Three faces ship, each in its own object so a program
// links only the ones it uses:
//
//   font5x7()     the thin 5x7 face, capitals from space to '_' (lowercase draws as capitals);
//   fontPanel7()  the heavy 7-pixel face: digits, capitals, '-' and '.' (anything else is blank);
//   fontPt2()     the 8x8 tracker face, 128 glyphs (BSD 3-Clause, see THIRD_PARTY_LICENSES.md).
//
// A plugin can make its own BitmapFont from a table it owns; a glyph is blank when the table has no entry.
#pragma once

#include <cstdint>

#include "spux/Version.hpp"

namespace spux::inline v1::ui::retro {

struct BitmapFont {
    int width;   // pixel columns of a glyph
    int height;  // pixel rows of a glyph, and bytes per glyph
    int advance; // from the left edge of one glyph to the next at scale 1 (width plus the gap)
    int capTop;  // row of the first pixel of a capital, and its height: where a label is centered
    int capHeight;
    char first;               // the character of the first table entry
    int count;                // entries in the table
    const std::uint8_t* rows; // count * height bytes
    bool upperOnly;           // lowercase letters draw as capitals
    char fallback;            // drawn for a character the table lacks

    // The glyph's `height` row bytes, or null when the font has no entry for `c` (and no fallback).
    [[nodiscard]] const std::uint8_t* glyph(char c) const {
        int i = static_cast<unsigned char>(c);
        if (upperOnly && i >= 'a' && i <= 'z')
            i -= 'a' - 'A';
        i -= static_cast<unsigned char>(first);
        if (i < 0 || i >= count) {
            i = static_cast<unsigned char>(fallback) - static_cast<unsigned char>(first);
            if (i < 0 || i >= count)
                return nullptr;
        }
        return rows + i * height;
    }

    // Width of `text` in pixels at an integer scale: the glyphs and the gaps between them, not after the last.
    [[nodiscard]] int textWidth(const char* text, int scale = 1) const {
        int n = 0;
        for (; text && text[n]; ++n) {
        }
        return n == 0 ? 0 : ((n - 1) * advance + width) * scale;
    }
};

const BitmapFont& font5x7();
const BitmapFont& fontPanel7();
const BitmapFont& fontPt2();

} // namespace spux::inline v1::ui::retro
