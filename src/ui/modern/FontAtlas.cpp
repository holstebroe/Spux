#include "spux/ui/modern/FontAtlas.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "spux/ui/modern/InterFont.hpp"

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb/stb_truetype.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace spux::inline v1::ui::modern {

namespace {
// The code points baked: Basic Latin, Latin-1, general punctuation and the few symbols the UI uses.
std::vector<std::uint32_t> bakedCodePoints() {
    std::vector<std::uint32_t> v;
    for (std::uint32_t c = 0x20; c < 0x7F; ++c)
        v.push_back(c);
    for (std::uint32_t c = 0xA0; c < 0x100; ++c)
        v.push_back(c);
    for (std::uint32_t c : {0x2013u, 0x2014u, 0x2018u, 0x2019u, 0x201Cu, 0x201Du, 0x2022u, 0x2026u, 0x2212u, 0x2192u,
                            0x2190u, 0x2191u, 0x2193u, 0x25B8u, 0x25BEu})
        v.push_back(c);
    return v;
}
} // namespace

int FontAtlas::index(std::uint32_t cp) const {
    if (cp >= map_.size())
        return -1;
    return map_[cp];
}

bool FontAtlas::build() {
    codepoints_ = bakedCodePoints();
    map_.assign(0x2600, -1);
    for (std::size_t i = 0; i < codepoints_.size(); ++i)
        map_[codepoints_[i]] = static_cast<std::int32_t>(i);
    pixels_.assign(static_cast<std::size_t>(kSize) * kSize, 0);

    const unsigned char* data[2] = {kInterRegular, kInterSemiBold};
    int penX = 1, penY = 1, rowH = 0;
    for (int w = 0; w < 2; ++w) {
        stbtt_fontinfo font;
        if (!stbtt_InitFont(&font, data[w], stbtt_GetFontOffsetForIndex(data[w], 0)))
            return false;
        const float scale = stbtt_ScaleForPixelHeight(&font, kBakePx);
        int ascent = 0, descent = 0, gap = 0;
        stbtt_GetFontVMetrics(&font, &ascent, &descent, &gap);
        const float digitAdvance = [&] {
            int adv = 0, lsb = 0, widest = 0;
            for (int d = '0'; d <= '9'; ++d) {
                stbtt_GetCodepointHMetrics(&font, d, &adv, &lsb);
                widest = std::max(widest, adv);
            }
            return static_cast<float>(widest) * scale / kBakePx;
        }();
        glyphs_[w].assign(codepoints_.size(), Glyph{});
        for (std::size_t i = 0; i < codepoints_.size(); ++i) {
            const int cp = static_cast<int>(codepoints_[i]);
            Glyph& g = glyphs_[w][i];
            int adv = 0, lsb = 0;
            stbtt_GetCodepointHMetrics(&font, cp, &adv, &lsb);
            g.advance = (cp >= '0' && cp <= '9') ? digitAdvance : static_cast<float>(adv) * scale / kBakePx;
            int gw = 0, gh = 0, xoff = 0, yoff = 0;
            unsigned char* sdf = stbtt_GetCodepointSDF(&font, scale, cp, kSpread, kOnEdge,
                                                       static_cast<float>(kOnEdge) / static_cast<float>(kSpread), &gw,
                                                       &gh, &xoff, &yoff);
            if (!sdf)
                continue; // blank glyph (space)
            if (penX + gw + 1 > kSize) {
                penX = 1;
                penY += rowH + 1;
                rowH = 0;
            }
            if (penY + gh + 1 > kSize) {
                stbtt_FreeSDF(sdf, nullptr);
                return false; // the atlas is too small: a build-time mistake, caught by the atlas test
            }
            for (int y = 0; y < gh; ++y)
                std::memcpy(&pixels_[static_cast<std::size_t>(penY + y) * kSize + static_cast<std::size_t>(penX)],
                            sdf + static_cast<std::size_t>(y) * static_cast<std::size_t>(gw),
                            static_cast<std::size_t>(gw));
            stbtt_FreeSDF(sdf, nullptr);
            // a texel center sits half a texel inside the quad: sample from the texel edges
            g.u0 = static_cast<float>(penX) / kSize;
            g.v0 = static_cast<float>(penY) / kSize;
            g.u1 = static_cast<float>(penX + gw) / kSize;
            g.v1 = static_cast<float>(penY + gh) / kSize;
            g.x0 = static_cast<float>(xoff) / kBakePx;
            g.y0 = static_cast<float>(yoff) / kBakePx;
            g.x1 = static_cast<float>(xoff + gw) / kBakePx;
            g.y1 = static_cast<float>(yoff + gh) / kBakePx;
            g.valid = true;
            penX += gw + 1;
            rowH = std::max(rowH, gh);
        }
    }
    return true;
}

const FontAtlas::Glyph& FontAtlas::glyph(FontWeight w, std::uint32_t cp) const {
    int i = index(cp);
    if (i < 0)
        i = index('?');
    if (i < 0)
        return none_;
    return glyphs_[w == FontWeight::Bold ? 1 : 0][static_cast<std::size_t>(i)];
}

std::uint32_t FontAtlas::nextCodePoint(std::string_view s, std::size_t& i) {
    const auto b = [&](std::size_t k) { return static_cast<unsigned char>(s[k]); };
    const unsigned char c = b(i);
    if (c < 0x80) {
        ++i;
        return c;
    }
    const std::size_t len = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
    std::size_t n = 1; // bytes of this sequence that are really there
    while (n < (len ? len : 1u << 30) && i + n < s.size() && (b(i + n) & 0xC0) == 0x80)
        ++n;
    if (len == 0) { // a stray continuation byte or an invalid lead: take it with its continuations
        i += n;
        return '?';
    }
    std::uint32_t cp = c & (0xFFu >> (len + 1));
    for (std::size_t k = 1; k < n; ++k)
        cp = (cp << 6) | (b(i + k) & 0x3Fu);
    static constexpr std::uint32_t kMin[5] = {0, 0, 0x80, 0x800, 0x10000};
    const bool ok = n == len && cp >= kMin[len] && cp <= 0x10FFFF && (cp < 0xD800 || cp > 0xDFFF);
    i += n;
    return ok ? cp : '?';
}

std::shared_ptr<const FontAtlas> FontAtlas::shared() {
    static const std::shared_ptr<const FontAtlas> atlas = [] {
        auto a = std::make_shared<FontAtlas>();
        return a->build() ? std::shared_ptr<const FontAtlas>(std::move(a)) : nullptr;
    }();
    return atlas;
}

float FontAtlas::width(FontWeight w, std::string_view s) const {
    float x = 0;
    for (std::size_t i = 0; i < s.size();)
        x += glyph(w, nextCodePoint(s, i)).advance;
    return x;
}

} // namespace spux::inline v1::ui::modern
