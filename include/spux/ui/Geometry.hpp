// Colours and geometry of the UI (from Plumeflow `ui/IRenderer`). Coordinates are logical pixels;
// colours are 0xAARRGGBB with straight alpha.
#pragma once

#include <cstdint>

#include "spux/Version.hpp"

namespace spux::inline v1::ui {

using Color = std::uint32_t;

constexpr Color rgb(unsigned r, unsigned g, unsigned b, unsigned a = 255) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

constexpr Color withAlpha(Color c, float a) {
    const float v = a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a;
    return (static_cast<Color>(static_cast<float>(c >> 24) * v + 0.5f) << 24) | (c & 0xFFFFFFu);
}

constexpr Color mix(Color a, Color b, float t) {
    auto ch = [&](int sh) {
        const float x = static_cast<float>((a >> sh) & 255u), y = static_cast<float>((b >> sh) & 255u);
        return static_cast<Color>(x + (y - x) * t + 0.5f) << sh;
    };
    return ch(24) | ch(16) | ch(8) | ch(0);
}

struct Point {
    float x = 0.0f, y = 0.0f;
};

// Room a painter may draw outside its widget's bounds (shadows, glow, focus rings).
struct Margin {
    int left = 0, top = 0, right = 0, bottom = 0;
    static constexpr Margin all(int m) { return {m, m, m, m}; }
    static constexpr Margin xy(int x, int y) { return {x, y, x, y}; }
    friend constexpr bool operator==(const Margin&, const Margin&) = default;
};

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    [[nodiscard]] constexpr int right() const { return x + w; }
    [[nodiscard]] constexpr int bottom() const { return y + h; }
    [[nodiscard]] constexpr bool empty() const { return w <= 0 || h <= 0; }
    [[nodiscard]] constexpr bool contains(int px, int py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
    [[nodiscard]] constexpr bool contains(Point p) const {
        // floor, not truncation: -0.5 is outside a rectangle at 0
        const int px = p.x < 0.0f ? -1 - static_cast<int>(-p.x) : static_cast<int>(p.x);
        const int py = p.y < 0.0f ? -1 - static_cast<int>(-p.y) : static_cast<int>(p.y);
        return contains(px, py);
    }
    [[nodiscard]] constexpr Point centre() const {
        return {static_cast<float>(x) + static_cast<float>(w) * 0.5f,
                static_cast<float>(y) + static_cast<float>(h) * 0.5f};
    }
    [[nodiscard]] constexpr Rect intersect(const Rect& o) const {
        const int x0 = x > o.x ? x : o.x, y0 = y > o.y ? y : o.y;
        const int x1 = right() < o.right() ? right() : o.right(), y1 = bottom() < o.bottom() ? bottom() : o.bottom();
        return x1 > x0 && y1 > y0 ? Rect{x0, y0, x1 - x0, y1 - y0} : Rect{};
    }
    [[nodiscard]] constexpr bool overlaps(const Rect& o) const { return !intersect(o).empty(); }
    [[nodiscard]] constexpr Rect unite(const Rect& o) const {
        if (empty())
            return o;
        if (o.empty())
            return *this;
        const int x0 = x < o.x ? x : o.x, y0 = y < o.y ? y : o.y;
        const int x1 = right() > o.right() ? right() : o.right(), y1 = bottom() > o.bottom() ? bottom() : o.bottom();
        return {x0, y0, x1 - x0, y1 - y0};
    }
    // Shrunk by d on every side (never below zero size).
    [[nodiscard]] constexpr Rect inset(int d) const { return inset(d, d); }
    [[nodiscard]] constexpr Rect inset(int dx, int dy) const {
        const int nw = w - 2 * dx, nh = h - 2 * dy;
        return {x + dx, y + dy, nw > 0 ? nw : 0, nh > 0 ? nh : 0};
    }
    [[nodiscard]] constexpr Rect expand(const Margin& m) const {
        return {x - m.left, y - m.top, w + m.left + m.right, h + m.top + m.bottom};
    }
    friend constexpr bool operator==(const Rect&, const Rect&) = default;
};

enum class Blend { Alpha, Add };
enum class Align { Left, Centre, Right };
enum class FontWeight { Regular, Bold };

} // namespace spux::inline v1::ui
