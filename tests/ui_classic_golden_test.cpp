// Golden hash of the Classic drawing code (moved unchanged from Tearwash/Tannhauser/Acidus): a worn
// panel, lettering in both fonts, anti-aliased lines, shaded boxes and the 5x7 bitmap font are
// drawn and the pixels hashed. A change here is a change of how every Classic panel looks.
#include <doctest.h>

#include "spux/ui/classic/ClassicDraw.hpp"
#include "spux/ui/classic/DisplayFont.hpp"
#include "spux/ui/classic/Font.hpp"
#include "spux/ui/classic/LabelFont.hpp"

#include <vector>

namespace c = spux::ui::classic;

namespace {
uint64_t fnv1a(const std::vector<uint32_t>& px) {
    uint64_t h = 1469598103934665603ull;
    for (uint32_t v : px)
        for (int i = 0; i < 4; ++i) {
            h ^= (v >> (8 * i)) & 0xFF;
            h *= 1099511628211ull;
        }
    return h;
}
} // namespace

TEST_CASE("Classic drawing renders the same pixels") {
    constexpr int W = 160, H = 100, S = 2;
    std::vector<uint32_t> buf(static_cast<size_t>(W) * S * H * S, 0);
    c::Graphics g(buf.data(), W, H, S);
    c::paintWornPanel(g, W, H, true);
    c::drawText(g, c::kDisplayFont, "Spux 0123", 8.f, 8.f, 14.f, c::kInk, 0.3f);
    c::drawTextCentered(g, c::kLabelFont, "CUTOFF", 80.f, 40.f, 6.f, c::kInk);
    c::drawLineAA(g, 10.f, 60.f, 150.f, 90.f, 2.f, 0xFFB03020);
    const c::Material m{ c::kPaint, 0.3f, 0.7f, 0.4f, 16.f };
    c::shadeBox(g, 100.f, 50.f, 140.f, 70.f, [&](float lx, float ly) {
        return c::toArgb(c::shadeLit(m, c::tiltedNormal(0.6f, 0.8f, 0.01f * (lx + ly))), 1.f);
    });
    g.drawText(c::Font::classic5x7(), "A", 4, 80, 0xFF101010);
    g.drawCircle(30, 70, 8, 0xFF2040C0, 2);

    CHECK(c::Font::defaultPanelFont().getWidth() == 7);
    CHECK(c::textWidth(c::kLabelFont, "CUTOFF", 6.f) > 0.f);
    // Regenerate only when a Classic look change is intended; say so in the changelog line.
    CHECK(fnv1a(buf) == 0x5b43c5fa2e0ea164ull);
}
