// spux_ui_retro: the bitmap fonts, the pixel renderer (whole pixels, no coverage values), RetroStyle's painters (inside
// their bounds, with the geometry the interaction asks for), the three looks as golden frames, the Gui and the editor
// shell on top of them.
#include <doctest.h>

#include <cmath>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "ui/RetroSupport.hpp"
#include "ui/TestRenderer.hpp"

using namespace retro_test;

namespace {

std::uint32_t at(const std::vector<std::uint32_t>& px, int w, int x, int y) {
    return px[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)];
}

int diffCount(const std::vector<std::uint32_t>& a, const std::vector<std::uint32_t>& b) {
    if (a.size() != b.size())
        return -1;
    int n = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        n += a[i] != b[i];
    return n;
}

constexpr Color kBg = 0xFF000000u, kFg = 0xFFFFFFFFu;

// The distinct colors in a frame.
std::set<std::uint32_t> colorsOf(const std::vector<std::uint32_t>& px) { return {px.begin(), px.end()}; }

// The lit pixels of a frame drawn white on black, as rows of '#' and '.'.
std::vector<std::string> pattern(const PixelRenderer& r, int x, int y, int w, int h) {
    const auto px = r.frame();
    std::vector<std::string> rows;
    for (int j = 0; j < h; ++j) {
        std::string row;
        for (int i = 0; i < w; ++i)
            row += at(px, r.width(), x + i, y + j) == kFg ? '#' : '.';
        rows.push_back(row);
    }
    return rows;
}

} // namespace

// --- the fonts -----------------------------------------------------------------------------------------------

TEST_CASE("bitmap fonts: the three faces, their glyphs and their metrics") {
    const BitmapFont& f = font5x7();
    const BitmapFont& p = fontPanel7();
    const BitmapFont& t = fontPt2();
    CHECK((f.width == 5 && f.height == 7 && f.advance == 6));
    CHECK((p.width == 7 && p.height == 7 && p.advance == 8));
    CHECK((t.width == 8 && t.height == 8 && t.advance == 8));

    // the 5x7 'A', as the thin face has always drawn it
    const std::uint8_t a57[7] = {0x70, 0x88, 0x88, 0x88, 0xF8, 0x88, 0x88};
    REQUIRE(f.glyph('A') != nullptr);
    for (int r = 0; r < 7; ++r)
        CHECK(f.glyph('A')[r] == a57[r]);
    // lowercase draws as capitals; characters the table lacks draw the fallback
    CHECK(f.glyph('a') == f.glyph('A'));
    CHECK(f.glyph('~') == f.glyph('?'));
    CHECK(f.glyph('\x01') == f.glyph('?'));
    CHECK(f.glyph(' ')[3] == 0);
    CHECK(f.glyph('_') != nullptr); // the table runs to '_'
    // the 7-pixel face has digits, capitals, '-' and '.'; the rest is blank
    for (char c : std::string("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-."))
        CHECK(std::any_of(p.glyph(c), p.glyph(c) + 7, [](std::uint8_t b) { return b != 0; }));
    CHECK(std::all_of(p.glyph('%'), p.glyph('%') + 7, [](std::uint8_t b) { return b == 0; }));
    CHECK(p.glyph('z') == p.glyph('Z'));
    // the 8x8 tracker face: 'A' sits on rows 1 to 5 of its cell
    const std::uint8_t a8[8] = {0x00, 0x3c, 0x66, 0x7e, 0x66, 0x66, 0x00, 0x00};
    for (int r = 0; r < 8; ++r)
        CHECK(t.glyph('A')[r] == a8[r]);
    CHECK((t.capTop == 1 && t.capHeight == 5));
    // metrics: glyphs and the gaps between them, not after the last
    CHECK(f.textWidth("") == 0);
    CHECK(f.textWidth("A") == 5);
    CHECK(f.textWidth("ABC") == 17);
    CHECK(f.textWidth("ABC", 2) == 34);
    CHECK(t.textWidth("ABC") == 24);
    // every face's capitals are where it says: the first and last row of an 'H' lie inside capTop and capHeight
    for (const BitmapFont* font : {&f, &p, &t}) {
        int top = font->height, bottom = -1;
        for (int r = 0; r < font->height; ++r)
            if (font->glyph('H')[r]) {
                top = std::min(top, r);
                bottom = std::max(bottom, r);
            }
        CHECK(top == font->capTop);
        CHECK(bottom - top + 1 == font->capHeight);
    }
}

// --- the pixel renderer ---------------------------------------------------------------------------------------

TEST_CASE("pixel renderer: every basic draws whole pixels, never a coverage value") {
    PixelRenderer r(120, 90);
    r.clear(kBg);
    r.fillRect({3, 3, 20, 10}, kFg);
    r.strokeRect({30, 3, 20, 14}, kFg, 2.0f);
    r.fillRoundRect({60, 3, 24, 16}, 4.0f, kFg);
    r.strokeRoundRect({90, 3, 24, 16}, 5.0f, kFg, 2.0f);
    r.fillGradientRect({3, 25, 20, 12}, kFg, kBg);
    r.drawLine({30.3f, 25.7f}, {60.9f, 38.2f}, kFg, 1.0f);
    r.drawLine({30.0f, 40.0f}, {60.0f, 40.0f}, kFg, 3.0f);
    const Point poly[4] = {{70.0f, 30.0f}, {80.0f, 40.0f}, {90.0f, 28.0f}, {100.0f, 41.0f}};
    r.drawPolyline(poly, 4, kFg, 2.0f);
    r.fillCircle({15.5f, 55.5f}, 9.0f, kFg);
    r.strokeCircle({45.5f, 55.5f}, 9.0f, kFg, 2.0f);
    r.drawArc({75.5f, 55.5f}, 9.0f, 0.3f, 4.0f, kFg, 2.0f);
    r.drawText("Pixel TEXT 123", 3, 70, kFg, 1);
    r.drawText("BIG", 70, 70, kFg, 2);
    const auto colors = colorsOf(r.frame());
    // the gradient makes in-between greys; leave it out and the frame has exactly two colors
    PixelRenderer q(120, 90);
    q.clear(kBg);
    q.strokeRect({30, 3, 20, 14}, kFg, 2.0f);
    q.fillRoundRect({60, 3, 24, 16}, 4.0f, kFg);
    q.drawLine({30.3f, 25.7f}, {60.9f, 38.2f}, kFg, 1.0f);
    q.fillCircle({15.5f, 55.5f}, 9.0f, kFg);
    q.strokeCircle({45.5f, 55.5f}, 9.0f, kFg, 2.0f);
    q.drawArc({75.5f, 55.5f}, 9.0f, 0.3f, 4.0f, kFg, 2.0f);
    q.drawText("Pixel TEXT 123", 3, 70, kFg, 1);
    CHECK(colorsOf(q.frame()) == std::set<std::uint32_t>{kBg, kFg});
    CHECK(colors.size() > 2); // the gradient
}

TEST_CASE("pixel renderer: rectangles, strokes and rounded corners are exact") {
    PixelRenderer r(40, 30);
    r.clear(kBg);
    r.fillRect({2, 3, 5, 4}, kFg);
    auto px = r.frame();
    int n = 0;
    for (std::uint32_t p : px)
        n += p == kFg;
    CHECK(n == 20);
    CHECK(at(px, 40, 2, 3) == kFg);
    CHECK(at(px, 40, 6, 6) == kFg);
    CHECK(at(px, 40, 7, 6) == kBg);
    CHECK(at(px, 40, 6, 7) == kBg);

    // a stroke is inside the rectangle, `thickness` rounded to whole pixels, never less than one
    r.clear(kBg);
    r.strokeRect({0, 0, 8, 6}, kFg, 0.2f);
    CHECK(pattern(r, 0, 0, 8, 6) ==
          std::vector<std::string>{"########", "#......#", "#......#", "#......#", "#......#", "########"});
    r.clear(kBg);
    r.strokeRect({0, 0, 8, 8}, kFg, 2.0f);
    CHECK(pattern(r, 0, 0, 8, 8) == std::vector<std::string>{"########", "########", "##....##", "##....##", "##....##",
                                                             "##....##", "########", "########"});
    r.clear(kBg);
    r.strokeRect({0, 0, 4, 4}, kFg, 3.0f); // thicker than the box: it fills it
    CHECK(pattern(r, 0, 0, 4, 4)[1] == "####");

    // one pixel of radius cuts nothing, two take the corner pixel off, and the radius is clamped to the box
    r.clear(kBg);
    r.fillRoundRect({0, 0, 8, 6}, 1.0f, kFg);
    CHECK(pattern(r, 0, 0, 8, 1)[0] == "########");
    r.clear(kBg);
    r.fillRoundRect({0, 0, 8, 6}, 2.0f, kFg);
    CHECK(pattern(r, 0, 0, 8, 6) ==
          std::vector<std::string>{".######.", "########", "########", "########", "########", ".######."});
    r.clear(kBg);
    r.fillRoundRect({0, 0, 8, 6}, 40.0f, kFg);
    n = 0;
    for (const std::string& row : pattern(r, 0, 0, 8, 6))
        n += static_cast<int>(std::count(row.begin(), row.end(), '#'));
    CHECK(n < 48);
    CHECK(n > 30);
    r.clear(kBg);
    r.strokeRoundRect({0, 0, 10, 8}, 2.0f, kFg, 1.0f);
    const auto ring = pattern(r, 0, 0, 10, 8);
    CHECK(ring[0] == ".########.");
    CHECK(ring[1] == "#........#");
    CHECK(ring[3] == "#........#");
    CHECK(ring[7] == ".########.");

    // a vertical gradient: the first row is near the top color, the last near the bottom, monotonic in between
    r.clear(kBg);
    r.fillGradientRect({0, 0, 4, 20}, rgb(200, 0, 0), rgb(0, 0, 200));
    px = r.frame();
    CHECK(((at(px, 40, 1, 0) >> 16) & 255u) > 190);
    CHECK((at(px, 40, 1, 19) & 255u) > 190);
    for (int y = 1; y < 20; ++y)
        CHECK(((at(px, 40, 1, y) >> 16) & 255u) <= ((at(px, 40, 1, y - 1) >> 16) & 255u));
    // empty rectangles draw nothing
    r.clear(kBg);
    r.fillRect({5, 5, 0, 4}, kFg);
    r.fillRoundRect({5, 5, 4, 0}, 2.0f, kFg);
    r.strokeRect({5, 5, -3, 4}, kFg);
    CHECK(colorsOf(r.frame()) == std::set<std::uint32_t>{kBg});
}

TEST_CASE("pixel renderer: a circle covers the pixels whose centers are inside it, a line is a Bresenham line") {
    PixelRenderer r(30, 30);
    r.clear(kBg);
    // radius 3 around the pixel (10, 10): the 29 pixels with dx^2 + dy^2 <= 9
    r.fillCircle({10.5f, 10.5f}, 3.0f, kFg);
    const auto disc = pattern(r, 7, 7, 7, 7);
    CHECK(disc ==
          std::vector<std::string>{"...#...", ".#####.", ".#####.", "#######", ".#####.", ".#####.", "...#..."});
    int n = 0;
    for (std::uint32_t p : r.frame())
        n += p == kFg;
    CHECK(n == 29);
    // a ring of width one: the same disc less the pixels strictly inside the circle of radius two
    r.clear(kBg);
    r.strokeCircle({10.5f, 10.5f}, 3.0f, kFg, 1.0f);
    n = 0;
    for (std::uint32_t p : r.frame())
        n += p == kFg;
    CHECK(n == 29 - 9); // the 9 pixels with dx^2 + dy^2 < 4 stay empty
    CHECK(at(r.frame(), 30, 10, 10) == kBg);
    CHECK(at(r.frame(), 30, 13, 10) == kFg);

    // Bresenham: one pixel per step along the long axis, the end points drawn
    r.clear(kBg);
    r.drawLine({1.0f, 1.0f}, {10.0f, 4.0f}, kFg);
    n = 0;
    for (std::uint32_t p : r.frame())
        n += p == kFg;
    CHECK(n == 10);
    CHECK(at(r.frame(), 30, 1, 1) == kFg);
    CHECK(at(r.frame(), 30, 10, 4) == kFg);
    // a point is the pixel it lies in, and a line drawn backwards is the same line
    PixelRenderer q(30, 30);
    q.clear(kBg);
    q.drawLine({10.0f, 4.0f}, {1.0f, 1.0f}, kFg);
    CHECK(diffCount(r.frame(), q.frame()) == 0);
    r.clear(kBg);
    r.drawLine({5.9f, 5.1f}, {5.9f, 5.1f}, kFg);
    CHECK(at(r.frame(), 30, 5, 5) == kFg);
    // thickness: a brush of whole pixels; horizontal and vertical runs are rectangles
    r.clear(kBg);
    r.drawLine({4.0f, 10.0f}, {14.0f, 10.0f}, kFg, 3.0f);
    n = 0;
    for (std::uint32_t p : r.frame())
        n += p == kFg;
    CHECK(n == 39); // x from 3 to 15: the brush reaches one pixel beyond each end
    CHECK(at(r.frame(), 30, 4, 9) == kFg);
    CHECK(at(r.frame(), 30, 15, 11) == kFg);
    CHECK(at(r.frame(), 30, 16, 10) == kBg);
    r.clear(kBg);
    r.drawLine({4.0f, 4.0f}, {4.0f, 12.0f}, kFg, 1.0f);
    n = 0;
    for (std::uint32_t p : r.frame())
        n += p == kFg;
    CHECK(n == 9);
}

TEST_CASE("pixel renderer: clipping, alpha and the add blend, in integers") {
    PixelRenderer r(64, 48);
    r.clear(kBg);
    r.fillRect({10, 10, 20, 20}, rgb(255, 0, 0));
    r.pushClip({0, 0, 15, 15});
    r.fillRect({0, 0, 64, 48}, rgb(0, 255, 0));
    r.pushClip({12, 12, 30, 30}); // clips intersect
    r.fillRect({0, 0, 64, 48}, rgb(0, 0, 255));
    r.popClip();
    r.popClip();
    auto px = r.frame();
    CHECK(at(px, 64, 5, 5) == rgb(0, 255, 0));
    CHECK(at(px, 64, 13, 13) == rgb(0, 0, 255));
    CHECK(at(px, 64, 20, 20) == rgb(255, 0, 0));
    CHECK(at(px, 64, 40, 40) == kBg);
    // everything respects the clip: text, shapes, lines, single pixels
    r.clear(kBg);
    const Rect clip{20, 10, 24, 20};
    r.pushClip(clip);
    r.fillRoundRect({0, 0, 64, 48}, 6.0f, rgb(200, 100, 50));
    r.strokeCircle({32.0f, 20.0f}, 30.0f, rgb(255, 255, 255), 3.0f);
    r.drawLine({-10.0f, -10.0f}, {100.0f, 70.0f}, rgb(255, 0, 0), 3.0f);
    r.drawText("CLIPPED TEXT RUNS PAST THE EDGE", 0, 15, rgb(255, 255, 255), 2);
    r.pixel(0, 0, rgb(255, 255, 255));
    r.drawGlow({32.0f, 20.0f}, 40.0f, rgb(255, 255, 0));
    r.drawShadow({10, 5, 50, 30}, 4.0f, 12.0f, rgb(0, 255, 255));
    r.popClip();
    px = r.frame();
    int outside = 0, inside = 0;
    for (int y = 0; y < 48; ++y)
        for (int x = 0; x < 64; ++x) {
            const bool changed = at(px, 64, x, y) != kBg;
            (clip.contains(x, y) ? inside : outside) += changed;
        }
    CHECK(outside == 0);
    CHECK(inside > 200);

    // alpha: a rounded mean per channel; the frame stays opaque
    r.clear(rgb(0, 0, 0));
    r.fillRect({0, 0, 8, 8}, rgb(200, 100, 50, 128));
    CHECK(at(r.frame(), 64, 4, 4) == rgb(100, 50, 25));
    r.fillRect({0, 0, 8, 8}, rgb(255, 255, 255, 0)); // fully transparent: nothing
    CHECK(at(r.frame(), 64, 4, 4) == rgb(100, 50, 25));
    // add: saturating
    r.clear(rgb(100, 200, 10));
    r.setBlend(Blend::Add);
    r.fillRect({0, 0, 8, 8}, rgb(100, 100, 100));
    CHECK(at(r.frame(), 64, 4, 4) == rgb(200, 255, 110));
    r.setBlend(Blend::Alpha);
    r.fillRect({0, 0, 8, 8}, rgb(1, 2, 3));
    CHECK(at(r.frame(), 64, 4, 4) == rgb(1, 2, 3));
    r.pixel(-1, 5, kFg); // off the frame: nothing, no crash
    r.pixel(5, 99, kFg);
}

TEST_CASE("pixel renderer: text is the font's glyphs, at integer scales, aligned on the top of the capitals") {
    PixelRenderer r(60, 40);
    r.clear(kBg);
    r.drawText("A", 2, 3, kFg);
    const auto a = pattern(r, 2, 3, 5, 7);
    CHECK(a == std::vector<std::string>{".###.", "#...#", "#...#", "#...#", "#####", "#...#", "#...#"});
    CHECK(pattern(r, 2, 2, 5, 1)[0] == ".....");
    // twice the size: every pixel is a 2 x 2 block
    r.clear(kBg);
    r.drawText("A", 2, 3, kFg, 2);
    const auto big = pattern(r, 2, 3, 10, 14);
    for (int y = 0; y < 14; ++y)
        for (int x = 0; x < 10; ++x)
            CHECK(big[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] ==
                  a[static_cast<std::size_t>(y / 2)][static_cast<std::size_t>(x / 2)]);
    // a font with a gap above its capitals still puts the capitals at y
    PixelRenderer t(60, 40, fontPt2());
    t.clear(kBg);
    t.drawText("A", 2, 3, kFg);
    CHECK(pattern(t, 2, 3, 8, 1)[0] == "..####..");
    CHECK(pattern(t, 2, 2, 8, 1)[0] == "........");
    CHECK(t.textHeight() == 5);
    CHECK(r.textHeight(3) == 21);
    CHECK(r.textWidth("ABC") == 17);
    CHECK(r.textWidth("ABC", 2) == 34);

    // drawTextPx: whole-step scale from the pixel size, alignment from the width, bold from the bold font
    PixelRenderer p(100, 40, font5x7(), &fontPanel7());
    p.clear(kBg);
    p.drawTextPx("AB", 50.0f, 5.0f, kFg, 10.0f, Align::Center);
    const int w = font5x7().textWidth("AB");
    int leftmost = 100;
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 100; ++x)
            if (at(p.frame(), 100, x, y) == kFg)
                leftmost = std::min(leftmost, x);
    CHECK(leftmost == 50 - w / 2);
    CHECK(p.textWidthPx("AB", 10.0f) == static_cast<float>(w));
    CHECK(p.textWidthPx("AB", 20.0f) == static_cast<float>(2 * w));
    CHECK(p.textWidthPx("AB", 30.0f) == static_cast<float>(3 * w));
    CHECK(p.textWidthPx("AB", 10.0f, FontWeight::Bold) == static_cast<float>(fontPanel7().textWidth("AB")));
    CHECK(p.textWidthPx("AB", 10.0f) != p.textWidthPx("AB", 10.0f, FontWeight::Bold));
    p.clear(kBg);
    p.drawTextPx("AB", 90.0f, 5.0f, kFg, 10.0f, Align::Right);
    int rightmost = 0;
    const auto px = p.frame();
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 100; ++x)
            if (at(px, 100, x, y) == kFg)
                rightmost = std::max(rightmost, x);
    CHECK(rightmost == 89);
    // a renderer without a bold font draws bold as regular
    CHECK(r.textWidthPx("AB", 10.0f, FontWeight::Bold) == r.textWidthPx("AB", 10.0f));
    // text with nothing to draw, or nothing to draw it with
    r.clear(kBg);
    r.drawText("", 5, 5, kFg);
    r.text(font5x7(), nullptr, 5, 5, kFg);
    CHECK(colorsOf(r.frame()) == std::set<std::uint32_t>{kBg});
    // bits: a one-bit picture, at a scale
    const std::uint8_t arrow[4] = {0x10, 0x30, 0x70, 0x30};
    r.bits(10, 10, arrow, 4, 4, kFg, 2);
    CHECK(at(r.frame(), 60, 16, 10) == kFg); // column 3 of the first row, twice as wide
    CHECK(at(r.frame(), 60, 17, 11) == kFg);
    CHECK(at(r.frame(), 60, 15, 10) == kBg);
    CHECK(at(r.frame(), 60, 10, 10) == kBg);
}

TEST_CASE("pixel renderer: the pixel-art effects are dithered, stamped or segmented, and stay in their areas") {
    // the ordered pattern: the coverage of a block of 4 x 4 pixels is level / 16
    for (int level = 0; level <= 16; ++level) {
        int on = 0;
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                on += PixelRenderer::dither(x, y, level);
        CHECK(on == level);
    }
    PixelRenderer r(80, 60);
    r.clear(kBg);
    r.fillDither({0, 0, 16, 16}, kFg, 8);
    int on = 0;
    for (std::uint32_t p : r.frame())
        on += p == kFg;
    CHECK(on == 128);
    // a glow is dense in the middle and sparse at the edge, and only whole pixels
    r.clear(kBg);
    r.drawGlow({40.0f, 30.0f}, 20.0f, kFg);
    auto px = r.frame();
    auto count = [&](int cx, int cy, int half) {
        int n = 0;
        for (int y = cy - half; y < cy + half; ++y)
            for (int x = cx - half; x < cx + half; ++x)
                n += at(px, 80, x, y) == kFg;
        return n;
    };
    CHECK(count(40, 30, 4) > 35);
    CHECK(count(40, 30, 4) > 3 * (count(40, 30, 20) - count(40, 30, 16)) / 4);
    CHECK(colorsOf(px) == std::set<std::uint32_t>{kBg, kFg});
    CHECK(at(px, 80, 40 + 21, 30) == kBg); // and it ends at its radius
    // a radial gradient has four steps
    r.clear(kBg);
    r.fillRadialGradient({40.0f, 30.0f}, 20.0f, rgb(255, 255, 255), rgb(0, 0, 0));
    CHECK(colorsOf(r.frame()).size() <= 5);
    CHECK(at(r.frame(), 80, 40, 30) == kFg);
    // a shadow: solid under the box, a band of dither around it, nothing beyond
    r.clear(kBg);
    r.drawShadow({20, 15, 30, 20}, 3.0f, 10.0f, kFg);
    px = r.frame();
    CHECK(at(px, 80, 30, 25) == kFg);
    CHECK(at(px, 80, 20 - 6, 25) == kBg);
    int band = 0;
    for (int x = 12; x < 20; ++x)
        band += at(px, 80, x, 25) == kFg;
    CHECK(band >= 1);
    CHECK(band < 8);
    // an arc is one connected run of pixels from its start to its end
    r.clear(kBg);
    r.drawArc({40.5f, 30.5f}, 15.0f, 0.0f, 1.5707963f, kFg, 1.0f);
    px = r.frame();
    CHECK(at(px, 80, 55, 30) == kFg); // 3 o'clock
    CHECK(at(px, 80, 40, 45) == kFg); // 6 o'clock
    CHECK(at(px, 80, 25, 30) == kBg); // the other half is not drawn
    // a level bar fills segments from the bottom
    r.clear(kBg);
    r.drawMeter({10, 10, 6, 41}, 0.5f, kFg); // ten segments of three with a gap
    px = r.frame();
    CHECK(at(px, 80, 12, 49) == kFg);
    CHECK(at(px, 80, 12, 51) == kBg); // below the bar
    CHECK(at(px, 80, 12, 10) != kFg);
    // the glass panel is a flat tint with a bevel
    r.clear(kBg);
    r.drawPanel({10, 10, 20, 12}, 5.0f, rgb(60, 60, 120));
    px = r.frame();
    CHECK(at(px, 80, 20, 16) == rgb(60, 60, 120));
    CHECK(at(px, 80, 20, 10) != rgb(60, 60, 120));
    CHECK(at(px, 80, 10, 10) != kBg);
}

TEST_CASE("pixel renderer: the layers behave as the editor expects") {
    PixelRenderer r(50, 50);
    REQUIRE(r.layers() != nullptr);
    CHECK_FALSE(r.isGpu());
    r.cpu().clearDirty();
    r.cpu().markDirty({10, 10, 5, 5});
    CHECK(r.cpu().dirtyBounds() == Rect{10, 10, 5, 5});
    CHECK(r.cpu().scale() == 1);
    CHECK(r.width() == 50);
    CHECK(r.height() == 50);
    // the frame is opaque and a clip does not move with the layer
    r.clear(rgb(9, 8, 7, 0));
    CHECK(at(r.frame(), 50, 3, 3) == rgb(9, 8, 7));
}

// --- the painters ----------------------------------------------------------------------------------------------

namespace {
struct PaintCase {
    std::string name;
    Margin margin;
    std::function<void(IRenderer&, const Rect&)> paint;
};
} // namespace

TEST_CASE("every painter draws inside its bounds plus its margin, in every look") {
    for (int look = 0; look < 3; ++look) {
        RetroStyle style(lookConfig(look));
        const Theme* th = &style.theme;
        const Color own = rgb(232, 150, 40);
        const params::ParamInfo* info = &kInfos[0]; // quality: Enum
        const char* labels[3] = {"Aaaaaaaaaaaaaaaa", "Bbbbbbbbbbbbbbbbbbb", "C"};

        std::vector<PaintCase> cases;
        for (int v = 0; v < 6; ++v)
            cases.push_back(
                {"knob v=" + std::to_string(v), style.knob.margin(), [=, &style](IRenderer& r, const Rect& b) {
                     KnobState s;
                     s.theme = th;
                     s.label = "A VERY LONG LABEL THAT CANNOT FIT";
                     s.valueText = "12345.678 kHz and more";
                     s.accent = v % 2 ? own : th->colors.accent;
                     s.value = v == 1 ? 0.0 : v == 2 ? 1.0 : 0.63;
                     s.hot = v == 3;
                     s.focused = v == 3 || v == 4;
                     s.bipolar = v == 4;
                     s.dimmed = v == 5;
                     s.editing = v == 5;
                     s.editText = "-12.5 dB entered long text";
                     s.caret = true;
                     s.pickable = v == 2 || v == 4;
                     s.mod.any = v >= 3;
                     s.mod.base = 0.4;
                     s.mod.shared = v == 4 ? 0.1 : 0.9;
                     style.knob.paint(r, b, s);
                 }});
        for (int vertical = 0; vertical < 2; ++vertical)
            for (int v = 0; v < 3; ++v)
                cases.push_back({vertical ? "slider vertical" : "slider horizontal", style.slider.margin(),
                                 [=, &style](IRenderer& r, const Rect& b) {
                                     SliderState s;
                                     s.theme = th;
                                     s.vertical = vertical != 0;
                                     s.value = v == 0 ? 0.0 : v == 1 ? 0.5 : 1.0;
                                     s.param = v == 1 ? &kInfos[4] : nullptr; // osc_a_level: bipolar
                                     s.label = v == 2 ? "" : "A long label for the slider";
                                     s.valueText = v == 1 ? "" : "+12.34 dB value";
                                     s.accent = v == 2 ? own : th->colors.accent;
                                     s.hot = v == 1;
                                     s.focused = v == 1;
                                     style.slider.paint(r, b, s);
                                 }});
        for (int on = 0; on < 2; ++on)
            cases.push_back({"toggle", style.toggle.margin(), [=, &style](IRenderer& r, const Rect& b) {
                                 ToggleState s;
                                 s.theme = th;
                                 s.on = on != 0;
                                 s.label = "A toggle label that is too long to fit";
                                 s.accent = on ? own : 0;
                                 s.hot = on != 0;
                                 s.focused = true;
                                 style.toggle.paint(r, b, s);
                             }});
        for (int vertical = 0; vertical < 2; ++vertical)
            cases.push_back({"selector", style.selector.margin(), [=, &style](IRenderer& r, const Rect& b) {
                                 SelectorState s;
                                 s.theme = th;
                                 s.count = 3;
                                 s.index = 1;
                                 s.hotIndex = 2;
                                 s.labels = labels;
                                 s.vertical = vertical != 0;
                                 s.accent = own;
                                 s.focused = true;
                                 style.selector.paint(r, b, s);
                                 s.labels = nullptr;
                                 s.param = info;
                                 style.selector.paint(r, b, s);
                                 s.param = nullptr;
                                 style.selector.paint(r, b, s);
                             }});
        for (int variant :
             {static_cast<int>(kButtonPlain), static_cast<int>(kButtonCap), static_cast<int>(kButtonArrowLeft),
              static_cast<int>(kButtonArrowRight), static_cast<int>(kButtonDisplay)})
            for (int on = 0; on < 2; ++on)
                cases.push_back({"button v=" + std::to_string(variant), style.button.margin(),
                                 [=, &style](IRenderer& r, const Rect& b) {
                                     ButtonState s;
                                     s.theme = th;
                                     s.variant = variant;
                                     s.on = on != 0;
                                     s.hot = on == 0;
                                     s.active = on != 0;
                                     s.focused = true;
                                     s.label = variant == kButtonCap ? "A LONG LEGEND\nOVER TWO LINES"
                                                                     : "A BUTTON WITH A LONG LABEL";
                                     s.accent = own;
                                     style.button.paint(r, b, s);
                                 }});
        for (int titled = 0; titled < 2; ++titled)
            cases.push_back({titled ? "panel frame" : "panel surface", style.panel.margin(),
                             [=, &style](IRenderer& r, const Rect& b) {
                                 PanelState s;
                                 s.theme = th;
                                 s.title = titled ? "A panel title that is far too long for the frame" : "";
                                 style.panel.paint(r, b, s);
                             }});
        for (int on = 0; on < 2; ++on)
            cases.push_back({"led", style.led.margin(), [=, &style](IRenderer& r, const Rect& b) {
                                 LedState s;
                                 s.theme = th;
                                 s.on = on != 0;
                                 s.level = on ? 0.6f : 1.0f;
                                 s.label = "A LAMP WITH A LONG LABEL";
                                 s.accent = own;
                                 style.led.paint(r, b, s);
                             }});
        for (int v = 0; v < 3; ++v)
            cases.push_back({"keyboard", style.keyboard.margin(), [=, &style](IRenderer& r, const Rect& b) {
                                 std::uint8_t down[128] = {}, sounding[128] = {};
                                 down[v == 1 ? 49 : 48] = 1;
                                 sounding[52] = sounding[v == 2 ? 70 : 66] = 1;
                                 KeyboardState s;
                                 s.theme = th;
                                 s.first = v == 2 ? 47 : 48; // a keyboard may begin on a black key
                                 s.last = v == 2 ? 70 : 71;
                                 s.down = down;
                                 s.lit = sounding;
                                 s.hotKey = v == 0 ? 50 : -1;
                                 s.accent = own;
                                 style.keyboard.paint(r, b, s);
                             }});
        for (int open = 0; open < 2; ++open) {
            cases.push_back({"dropdown", style.dropdown.margin(), [=, &style](IRenderer& r, const Rect& b) {
                                 DropdownState s;
                                 s.theme = th;
                                 s.label = "TYPE";
                                 s.valueText = "A value text that is much too long";
                                 s.open = open != 0;
                                 s.hot = open == 0;
                                 s.focused = true;
                                 style.dropdown.paint(r, b, s);
                             }});
            cases.push_back({"stepper", style.stepper.margin(), [=, &style](IRenderer& r, const Rect& b) {
                                 StepperState s;
                                 s.theme = th;
                                 s.label = "Semitones";
                                 s.valueText = "+12 st and then some";
                                 s.hotPart = open ? 1 : -1;
                                 s.focused = true;
                                 style.stepper.paint(r, b, s);
                             }});
        }
        cases.push_back({"text field", style.textField.margin(), [=, &style](IRenderer& r, const Rect& b) {
                             TextFieldState s;
                             s.theme = th;
                             s.text = "Some typed text that is much too long for the field";
                             s.caret = true;
                             style.textField.paint(r, b, s);
                         }});
        cases.push_back({"icons", style.button.margin(), [=, &style](IRenderer& r, const Rect& b) {
                             ButtonState s;
                             s.theme = th;
                             s.icon = waveIcon(Wave::Saw);
                             s.label = "SAW WITH A LONG LABEL";
                             style.button.paint(r, b, s);
                             s.label = "";
                             s.variant = kButtonCap;
                             style.button.paint(r, b, s);
                             const Icon icons[3] = {waveIcon(Wave::Sine), waveIcon(Wave::Pulse), waveIcon(Wave::Noise)};
                             SelectorState se;
                             se.theme = th;
                             se.count = 3;
                             se.index = 1;
                             se.icons = icons;
                             style.selector.paint(r, b, se);
                             se.vertical = true;
                             style.selector.paint(r, b, se);
                         }});
        cases.push_back({"tabs", style.tabs.margin(), [=, &style](IRenderer& r, const Rect& b) {
                             static const char* const labels[3] = {"LFO 1", "A VERY LONG TAB NAME", "MOD"};
                             TabsState s;
                             s.theme = th;
                             s.count = 3;
                             s.index = 1;
                             s.hotIndex = 2;
                             s.labels = labels;
                             s.focused = true;
                             s.accent = own;
                             style.tabs.paint(r, b, s);
                         }});
        for (int idle = 0; idle < 2; ++idle)
            cases.push_back({"readout", style.readout.margin(), [=, &style](IRenderer& r, const Rect& b) {
                                 ReadoutState s;
                                 s.theme = th;
                                 s.idle = idle != 0;
                                 s.name = idle ? "TOUCH A CONTROL TO SEE ITS VALUE HERE" : "A LONG PARAMETER NAME";
                                 s.value = "-12.5 dB and then some";
                                 s.hint = "Drag up or down; shift for fine";
                                 style.readout.paint(r, b, s);
                             }});

        const Rect sizes[] = {
            {60, 50, 120, 100}, {70, 60, 70, 40}, {60, 60, 150, 28}, {80, 70, 3, 3}, {64, 64, 40, 12}};
        for (const PaintCase& pc : cases)
            for (const Rect& b : sizes) {
                PixelRenderer r(280, 240, *style.config().font, style.config().titleFont);
                r.clear(rgb(77, 88, 99));
                pc.paint(r, b);
                const auto px = r.frame();
                const Rect allowed = b.expand(pc.margin);
                int outside = 0, inside = 0;
                for (int y = 0; y < r.height(); ++y)
                    for (int x = 0; x < r.width(); ++x) {
                        const bool changed = at(px, r.width(), x, y) != rgb(77, 88, 99);
                        if (!allowed.contains(x, y))
                            outside += changed;
                        else
                            inside += changed;
                    }
                INFO(lookName(look) << " " << pc.name << " in " << b.x << "," << b.y << " " << b.w << "x" << b.h);
                CHECK(outside == 0);
                if (b.w >= 70 && b.h >= 40)
                    CHECK(inside > 0);
            }
    }
}

TEST_CASE("a painter on another renderer draws nothing") {
    RetroStyle style;
    uitest::TestRenderer r(100, 100);
    KnobState k;
    k.theme = &style.theme;
    style.knob.paint(r, {0, 0, 80, 80}, k);
    SliderState sl;
    sl.theme = &style.theme;
    style.slider.paint(r, {0, 0, 80, 80}, sl);
    ToggleState t;
    t.theme = &style.theme;
    style.toggle.paint(r, {0, 0, 80, 80}, t);
    SelectorState se;
    se.theme = &style.theme;
    se.count = 2;
    style.selector.paint(r, {0, 0, 80, 80}, se);
    ButtonState b;
    b.theme = &style.theme;
    style.button.paint(r, {0, 0, 80, 80}, b);
    PanelState p;
    p.theme = &style.theme;
    style.panel.paint(r, {0, 0, 80, 80}, p);
    LedState l;
    l.theme = &style.theme;
    style.led.paint(r, {0, 0, 80, 20}, l);
    KeyboardState kb;
    kb.theme = &style.theme;
    style.keyboard.paint(r, {0, 0, 80, 40}, kb);
    DropdownState dd;
    dd.theme = &style.theme;
    style.dropdown.paint(r, {0, 0, 80, 20}, dd);
    StepperState st;
    st.theme = &style.theme;
    style.stepper.paint(r, {0, 0, 80, 20}, st);
    TextFieldState tf;
    tf.theme = &style.theme;
    style.textField.paint(r, {0, 0, 80, 20}, tf);
    TabsState tb;
    tb.theme = &style.theme;
    tb.count = 2;
    style.tabs.paint(r, {0, 0, 80, 20}, tb);
    ReadoutState ro;
    ro.theme = &style.theme;
    style.readout.paint(r, {0, 0, 80, 20}, ro);
    CHECK(r.calls == 0);
}

TEST_CASE("popup and tooltip: layout stays in view, items are placed, painting stays inside, in every look") {
    for (int look = 0; look < 3; ++look) {
        RetroStyle style(lookConfig(look));
        PixelRenderer r(400, 260, *style.config().font, style.config().titleFont);
        std::vector<PopupItem> items;
        for (int i = 0; i < 18; ++i)
            items.push_back({"Item " + std::to_string(i)});
        items.insert(items.begin() + 3, PopupItem{"", true, true});
        items.push_back({"Disabled", false, false});
        PopupState ps;
        ps.theme = &style.theme;
        ps.items = &items;
        ps.anchor = {350, 230, 40, 20};
        ps.columns = 3;
        ps.selected = 2;
        std::vector<Rect> rects;
        const Rect box = style.popup.layout(r, ps, 400, 260, rects);
        INFO(lookName(look));
        CHECK(Rect{0, 0, 400, 260}.intersect(box) == box);
        REQUIRE(rects.size() == items.size());
        for (const Rect& it : rects)
            CHECK(box.intersect(it) == it);
        CHECK(rects[3].h < rects[2].h); // the separator is a thin row
        ps.itemRects = &rects;
        ps.hot = 5;
        r.clear(rgb(77, 88, 99));
        style.popup.paint(r, box, ps);
        const auto px = r.frame();
        const Rect allowed = box.expand(style.popup.margin());
        int outside = 0, inside = 0;
        for (int y = 0; y < 260; ++y)
            for (int x = 0; x < 400; ++x) {
                const bool changed = at(px, 400, x, y) != rgb(77, 88, 99);
                outside += !allowed.contains(x, y) && changed;
                inside += allowed.contains(x, y) && changed;
            }
        CHECK(outside == 0);
        CHECK(inside > 3000);

        TooltipState ts;
        ts.theme = &style.theme;
        ts.title = "Cutoff";
        ts.text = "1.20 kHz   modulatable";
        ts.pointer = {390, 250};
        const Rect tip = style.tooltip.layout(r, ts, 400, 260);
        CHECK(Rect{0, 0, 400, 260}.intersect(tip) == tip);
        CHECK_FALSE(tip.contains(390, 250)); // not under the pointer
        r.clear(rgb(77, 88, 99));
        style.tooltip.paint(r, tip, ts);
        const auto tp = r.frame();
        const Rect tipAllowed = tip.expand(style.tooltip.margin());
        int tipOutside = 0;
        for (int y = 0; y < 260; ++y)
            for (int x = 0; x < 400; ++x)
                tipOutside += !tipAllowed.contains(x, y) && at(tp, 400, x, y) != rgb(77, 88, 99);
        CHECK(tipOutside == 0);
        // a long body never makes the tooltip wider than the view
        ts.text = "a very long description of a parameter that is much wider than this small view could ever hold";
        const Rect wide = style.tooltip.layout(r, ts, 200, 260);
        CHECK(Rect{0, 0, 200, 260}.intersect(wide) == wide);
    }
}

// --- icons -----------------------------------------------------------------------------------------------------

namespace {
int customCalls = 0;
Rect customArea;
const void* customData = nullptr;
void customIcon(IRenderer& r, const Rect& area, Color c, float width, const void* data) {
    ++customCalls;
    customArea = area;
    customData = data;
    r.fillRect(area.inset(2), withAlpha(c, width > 0 ? 1.0f : 0.0f));
}
} // namespace

TEST_CASE("icons: every wave draws on the pixel renderer inside its area, and a plugin's own icon is called") {
    std::vector<std::vector<std::uint32_t>> frames;
    for (int w = 0; w < kWaveCount; ++w) {
        PixelRenderer r(60, 40);
        r.clear(kBg);
        const Rect area{10, 10, 40, 20};
        drawWave(r, static_cast<Wave>(w), area, kFg, 1.0f);
        const auto px = r.frame();
        int on = 0, outside = 0;
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 60; ++x)
                if (at(px, 60, x, y) != kBg) {
                    ++on;
                    outside += !area.contains(x, y);
                }
        INFO(waveName(static_cast<Wave>(w)));
        CHECK(on > 20);
        CHECK(outside == 0);
        frames.push_back(px);
    }
    for (int a = 0; a < kWaveCount; ++a)
        for (int b = a + 1; b < kWaveCount; ++b)
            CHECK(diffCount(frames[static_cast<std::size_t>(a)], frames[static_cast<std::size_t>(b)]) > 10);

    int token = 0;
    Icon mine{customIcon, &token};
    RetroStyle style;
    PixelRenderer rr(100, 50);
    ButtonState s;
    s.theme = &style.theme;
    s.icon = mine;
    style.button.paint(rr, {10, 10, 80, 30}, s);
    CHECK(customCalls == 1);
    CHECK(customData == &token);
    CHECK(Rect{10, 10, 80, 30}.intersect(customArea) == customArea);
    const Icon steps[2] = {mine, waveIcon(Wave::Sine)};
    SelectorState se;
    se.theme = &style.theme;
    se.count = 2;
    se.icons = steps;
    style.selector.paint(rr, {10, 10, 80, 30}, se);
    CHECK(customCalls == 2);
}

// --- geometry the interaction asks for -----------------------------------------------------------------------------

TEST_CASE(
    "the knob's hit area is the dial and follows the rows; the slider's track and selector cells follow the painters") {
    RetroStyle silver;
    const Rect b{40, 30, 72, 80};
    const Rect dial = silver.knob.hitArea(b);
    CHECK(b.intersect(dial) == dial);
    CHECK(dial.w == dial.h);
    CHECK(dial.w % 2 == 1); // centered on a pixel
    CHECK(dial.center().x == doctest::Approx(76.5f).epsilon(0.1));
    RetroConfig rows = RetroConfig::silver();
    rows.knob.topRow = rows.knob.bottomRow = 0;
    RetroStyle bare(rows);
    CHECK(bare.knob.hitArea(b).h > dial.h); // no rows, a larger dial
    CHECK(bare.knob.hitArea(b).center().y == doctest::Approx(70.5f).epsilon(0.05));

    SliderState v;
    v.vertical = true;
    const Rect t = silver.slider.track({10, 10, 30, 156}, v);
    CHECK(t.x >= 10);
    CHECK(t.right() <= 40);
    CHECK(t.y >= 10);
    CHECK(t.bottom() <= 166);
    CHECK(t.h > 100);
    SliderState h;
    const Rect th = silver.slider.track({10, 10, 130, 18}, h);
    CHECK(th.w > th.h);

    for (int vertical = 0; vertical < 2; ++vertical) {
        SelectorState s;
        s.count = 3;
        s.vertical = vertical != 0;
        const Rect sel{10, 10, 80, 90};
        int total = 0;
        for (int i = 0; i < 3; ++i) {
            const Rect cell = silver.selector.cell(sel, s, i);
            CHECK(sel.intersect(cell) == cell);
            CHECK_FALSE(cell.empty());
            total += vertical ? cell.h : cell.w;
        }
        CHECK(total <= (vertical ? 90 : 80));
        CHECK(total >= (vertical ? 80 : 78));
    }
}

TEST_CASE("the slider's cap sits where its track says: the value maps onto the travel of the cap's center") {
    RetroStyle style;
    const Rect b{10, 10, 30, 156};
    SliderState s;
    s.vertical = true;
    const Rect tr = style.slider.track(b, s);
    for (double v : {0.0, 0.5, 1.0}) {
        s.value = v;
        s.theme = &style.theme;
        PixelRenderer r(60, 180);
        r.clear(kBg);
        style.slider.paint(r, b, s);
        // the cap is the widest run of lit pixels: its middle row is where the groove line is
        const auto px = r.frame();
        int rowOfWidest = -1, widest = 0;
        for (int y = b.y; y < b.bottom(); ++y) {
            int n = 0;
            for (int x = b.x; x < b.right(); ++x)
                n += at(px, 60, x, y) != kBg;
            if (n > widest) {
                widest = n;
                rowOfWidest = y;
            }
        }
        const int expected = tr.bottom() - static_cast<int>(std::lround(v * tr.h));
        INFO("value " << v);
        CHECK(std::abs(rowOfWidest - expected) <= style.config().slider.capLong / 2 + 3);
    }
}

// --- through the Gui -----------------------------------------------------------------------------------------------

TEST_CASE(
    "RetroStyle through the Gui: a knob drag is one gesture, a selector picks, a toggle flips, a button presses") {
    for (int look = 0; look < 3; ++look) {
        Host host;
        RetroStyle style(lookConfig(look));
        PixelRenderer r(kPageW, kPageH, *style.config().font, style.config().titleFont);
        RGui gui(host, style);
        gui.setCaching(false);
        auto frame = [&] {
            r.clear(style.theme.colors.background);
            gui.begin(r, 1.0, 0.016f, kPageW, kPageH);
            showcaseWidgets(gui);
            gui.end();
        };
        INFO(lookName(look));
        frame();
        const Rect knob = R(14, 22, 52, 72);
        const Point c0 = style.knob.hitArea(knob).center();
        const double before = host.plainOf(P_osc_a_phase_motion_rate);
        gui.onMouseDown(c0.x, c0.y, MouseButton::Left, false);
        frame();
        gui.onMouseMove(c0.x, c0.y - 40.0f);
        frame();
        gui.onMouseUp(c0.x, c0.y - 40.0f, MouseButton::Left);
        frame();
        const auto kinds = host.drain();
        REQUIRE(kinds.size() >= 3);
        CHECK(kinds.front() == ParamEdit::Kind::Begin);
        CHECK(kinds.back() == ParamEdit::Kind::End);
        CHECK(host.plainOf(P_osc_a_phase_motion_rate) > before);

        // the horizontal selector's cells are the painter's: a press in the third cell sets the third value
        const Rect sel = R(178, 204, 150, 20);
        SelectorState three;
        three.count = 3;
        const Rect third = style.selector.cell(sel, three, 2);
        const float px = static_cast<float>(third.x + third.w / 2), py = static_cast<float>(third.y + third.h / 2);
        gui.onMouseDown(px, py, MouseButton::Left, false);
        frame();
        gui.onMouseUp(px, py, MouseButton::Left);
        frame();
        CHECK(host.plainOf(P_quality) == 2.0);

        // the vertical lever's cells: the first step
        SelectorState lever;
        lever.count = 3;
        lever.vertical = true;
        const Rect first = style.selector.cell(R(114, 200, 56, 66), lever, 0);
        const float lx = static_cast<float>(first.x + first.w / 2), ly = static_cast<float>(first.y + first.h / 2);
        gui.onMouseDown(lx, ly, MouseButton::Left, false);
        frame();
        gui.onMouseUp(lx, ly, MouseButton::Left);
        frame();
        CHECK(host.plainOf(P_quality) == 0.0);

        // a press on a toggle flips it
        const double mono = host.plainOf(P_mono);
        gui.onMouseDown(38.0f, 243.0f, MouseButton::Left, false);
        frame();
        gui.onMouseUp(38.0f, 243.0f, MouseButton::Left);
        frame();
        CHECK(host.plainOf(P_mono) != mono);

        // a button reports its press
        bool pressed = false;
        auto bframe = [&] {
            r.clear(style.theme.colors.background);
            gui.begin(r, 1.0, 0.016f, kPageW, kPageH);
            pressed = gui.button("plain", R(334, 72, 62, 16), "PLAIN") || pressed;
            gui.end();
        };
        bframe();
        gui.onMouseDown(360.0f, 90.0f, MouseButton::Left, false);
        bframe();
        gui.onMouseUp(360.0f, 90.0f, MouseButton::Left);
        bframe();
        CHECK(pressed);
    }
}

TEST_CASE("the horizontal slider and the vertical one follow a press and a drag") {
    Host host;
    RetroStyle style;
    PixelRenderer r(kPageW, kPageH);
    RGui gui(host, style);
    gui.setCaching(false);
    const Rect vb = R(252, 22, 38, 156), hb = R(346, 24, 118, 18);
    auto frame = [&] {
        r.clear(style.theme.colors.background);
        gui.begin(r, 1.0, 0.016f, kPageW, kPageH);
        gui.slider(P_out_gain, vb, {.label = "GAIN", .vertical = true, .jump = true});
        gui.slider(P_env_amp_delay, hb, {.label = "DELAY", .jump = true});
        gui.end();
    };
    frame();
    SliderState v;
    v.vertical = true;
    const Rect tv = style.slider.track(vb, v);
    // a press at the middle of the travel puts the value near the middle of its range, at the top near the maximum
    const float mx = static_cast<float>(tv.x + tv.w / 2);
    gui.onMouseDown(mx, static_cast<float>(tv.bottom() - tv.h / 2), MouseButton::Left, false);
    frame();
    const double mid = host.plainOf(P_out_gain);
    gui.onMouseMove(mx, static_cast<float>(tv.y));
    frame();
    const double top = host.plainOf(P_out_gain);
    gui.onMouseUp(mx, static_cast<float>(tv.y), MouseButton::Left);
    frame();
    CHECK(top > mid);
    SliderState hs;
    const Rect th = style.slider.track(hb, hs);
    const float my = static_cast<float>(th.y + th.h / 2);
    gui.onMouseDown(static_cast<float>(th.x + th.w), my, MouseButton::Left, false);
    frame();
    const double right = host.plainOf(P_env_amp_delay);
    gui.onMouseMove(static_cast<float>(th.x), my);
    frame();
    const double left = host.plainOf(P_env_amp_delay);
    gui.onMouseUp(static_cast<float>(th.x), my, MouseButton::Left);
    CHECK(right > left);
}

TEST_CASE("keyboard: sounding and held keys change what is drawn; the Gui reports a press") {
    Host host;
    RetroStyle style;
    PixelRenderer r(400, 100);
    RGui gui(host, style);
    gui.setCaching(false);
    const Rect b{10, 10, 280, 80};
    KeyboardEvents last;
    auto frame = [&] {
        r.clear(style.theme.colors.background);
        gui.begin(r, 1.0, 0.016f, 400, 100);
        last = gui.keyboard("kb", b, {.first = 48, .last = 71});
        gui.end();
    };
    frame();
    gui.onMouseDown(20.0f, 80.0f, MouseButton::Left, false);
    frame();
    CHECK(last.pressed == 48);
    gui.onMouseUp(20.0f, 80.0f, MouseButton::Left);
    frame();
    CHECK(last.released == 48);

    std::uint8_t sounding[128] = {};
    sounding[52] = 1;
    PixelRenderer a(400, 100), c(400, 100);
    for (PixelRenderer* rr : {&a, &c}) {
        rr->clear(rgb(0, 0, 0));
        RGui g2(host, style);
        g2.setCaching(false);
        g2.begin(*rr, 1.0, 0.016f, 400, 100);
        g2.keyboard("kb", b, {.first = 48, .last = 71, .lit = rr == &c ? sounding : nullptr});
        g2.end();
    }
    CHECK(diffCount(a.frame(), c.frame()) > 50);
}

TEST_CASE("tabs, dropdown, stepper and the preset bar work through the Gui in a Retro style") {
    Host host;
    ShowPresets presets;
    host.presetHost = &presets;
    RetroStyle style;
    PixelRenderer r(400, 140);
    RGui gui(host, style);
    gui.setCaching(false);
    static const char* const labels[3] = {"ONE", "TWO", "THREE"};
    int tab = 0;
    const Rect bar{10, 10, 300, 18}, dd{10, 40, 120, 18}, step{10, 64, 120, 18}, pb{10, 90, 300, 18};
    PresetBarResult last;
    auto frame = [&] {
        r.clear(style.theme.colors.background);
        gui.begin(r, 1.0, 0.016f, 400, 140);
        tab = gui.tabs("tabs", bar, labels, 3, tab);
        gui.dropdown(P_quality, dd, "TYPE");
        gui.stepper(P_transpose, step, "SEMI");
        last = presetBar(gui, "p", pb);
        gui.end();
    };
    auto click = [&](float x, float y) {
        gui.onMouseDown(x, y, MouseButton::Left, false);
        frame();
        gui.onMouseUp(x, y, MouseButton::Left);
        frame();
    };
    frame();
    TabsState probe;
    probe.count = 3;
    const Rect third = style.tabs.cell(bar, probe, 2);
    click(static_cast<float>(third.x + 5), static_cast<float>(third.y + 5));
    CHECK(tab == 2);
    click(40.0f, 49.0f); // the dropdown opens its list
    CHECK(gui.popupOpen());
    gui.closePopup();
    frame();
    const double semi = host.plainOf(P_transpose);
    const Rect plus = style.stepper.plusPart(step);
    click(static_cast<float>(plus.x + plus.w / 2), static_cast<float>(plus.y + plus.h / 2));
    CHECK(host.plainOf(P_transpose) == semi + 1.0);
    CHECK(presets.current == 1);
    click(19.0f, 99.0f); // previous preset
    CHECK(presets.current == 0);
}

TEST_CASE("pick, modulation and focus: each changes the pixels of a knob") {
    for (int look = 0; look < 3; ++look) {
        RetroStyle style(lookConfig(look));
        auto render = [&](bool pick, bool mod, bool focus) {
            PixelRenderer r(100, 110, *style.config().font, style.config().titleFont);
            r.clear(style.theme.colors.background);
            KnobState s;
            s.theme = &style.theme;
            s.value = 0.4;
            s.pickable = pick;
            s.focused = focus;
            s.mod.any = mod;
            s.mod.base = 0.4;
            s.mod.shared = 0.9;
            style.knob.paint(r, {20, 10, 52, 72}, s);
            return r.frame();
        };
        INFO(lookName(look));
        const auto plain = render(false, false, false);
        CHECK(diffCount(plain, render(true, false, false)) > 20);
        CHECK(diffCount(plain, render(false, true, false)) > 10);
        CHECK(diffCount(plain, render(false, false, true)) > 10);
    }
}

// --- the looks -------------------------------------------------------------------------------------------------------

TEST_CASE("the config changes the look; configure() applies to the whole style; every look has its fonts") {
    Host host;
    auto render = [&](RetroStyle& s) {
        PixelRenderer r(kPageW, kPageH, *s.config().font, s.config().titleFont);
        showcaseFrame(r, host, s);
        return r.frame();
    };
    RetroStyle silver;
    const auto base = render(silver);
    RetroStyle tracker(RetroConfig::tracker());
    const auto trackerFrame = render(tracker);
    RetroStyle forge(RetroConfig::forge());
    const auto forgeFrame = render(forge);
    CHECK(diffCount(base, trackerFrame) > 30000);
    CHECK(diffCount(base, forgeFrame) > 30000);
    CHECK(diffCount(trackerFrame, forgeFrame) > 30000);

    CHECK(RetroConfig::silver().font == &font5x7());
    CHECK(RetroConfig::silver().titleFont == &fontPanel7());
    CHECK(RetroConfig::tracker().font == &fontPt2());
    CHECK(RetroConfig::forge().font == &font5x7());

    RetroStyle other;
    other.configure(RetroConfig::tracker());
    CHECK(other.theme.colors.accent == RetroConfig::tracker().theme.colors.accent);
    CHECK(diffCount(trackerFrame, render(other)) == 0);

    RetroConfig flat = RetroConfig::silver();
    flat.knob.shape = KnobShape::Flat;
    flat.toggle = ToggleShape::Key;
    RetroStyle custom(flat);
    const auto customFrame = render(custom);
    CHECK(diffCount(base, customFrame) > 1000);
    RetroStyle again;
    CHECK(diffCount(base, render(again)) == 0);
}

TEST_CASE("showcase: deterministic, filled, whole pixels, golden frames in every look") {
    Host host;
    for (int look = 0; look < 3; ++look) {
        RetroStyle style(lookConfig(look));
        const std::string name = lookName(look);
        PixelRenderer r(kPageW, kPageH, *style.config().font, style.config().titleFont);
        showcaseFrame(r, host, style);
        const auto a = r.frame();
        PixelRenderer r2(kPageW, kPageH, *style.config().font, style.config().titleFont);
        showcaseFrame(r2, host, style);
        INFO(name);
        CHECK(diffCount(a, r2.frame()) == 0);
        // a pixel-art page has a small palette: a few dozen colors, not thousands
        CHECK(colorsOf(a).size() < 120);
        CHECK(colorsOf(a).size() > 12);
        ui_test::snapshot(name, a, kPageW, kPageH);
        checkGoldenPixels(name, a, kPageW, kPageH, 0.002);
    }
}

TEST_CASE("the editor shell: cached frames equal uncached frames, and only what changed is redrawn") {
    for (int look = 0; look < 3; ++look) {
        Host host;
        attach(host);
        RetroStyle style(lookConfig(look));
        ShowRoot root;
        EditorOptions o;
        o.width = kPageW;
        o.height = kPageH;
        Editor<RetroStyle> ed(host, style, root, o, retroRenderer(style.config()));
        ed.gui().setOverlapCheck(true);
        REQUIRE(ed.frame(false, 0.0));
        INFO(lookName(look));
        CHECK(ed.gui().overlapCount() == 0);
        CHECK(ed.gui().idCollisions() == 0);
        CHECK_FALSE(ed.frame(false, 0.1));

        host.set(P_osc_a_phase_motion_rate, 1.0);
        REQUIRE(ed.frame(false, 0.2));
        CHECK(ed.gui().paintedCount() == 1);
        CHECK(ed.lastDirty().w < kPageW / 2); // one knob and what it overlaps, not the page
        const std::vector<std::uint32_t> cached(ed.pixels(), ed.pixels() + kPageW * kPageH);

        // the same state drawn from scratch: whole pixels, so the two agree exactly
        Host host2;
        host2.set(P_osc_a_phase_motion_rate, 1.0);
        PixelRenderer r(kPageW, kPageH, *style.config().font, style.config().titleFont);
        showcaseFrame(r, host2, style, 0.2);
        CHECK(diffCount(cached, r.frame()) == 0);
    }
}
