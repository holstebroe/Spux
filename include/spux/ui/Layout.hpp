// Layout helpers (guidelines 7, immediate mode cache 5): they return Rects and keep no state, so a page
// can compute its layout every frame or keep it as a table of data it loops over.
#pragma once

#include <array>
#include <cstddef>

#include "spux/ui/Geometry.hpp"

namespace spux::inline v1::ui {

// Cell i of n equal cells side by side in `r`, `gap` pixels apart.
constexpr Rect rowCell(const Rect& r, int n, int i, int gap = 0) {
    if (n <= 0)
        return {};
    const int w = r.w - gap * (n - 1);
    const int x0 = r.x + w * i / n + gap * i, x1 = r.x + w * (i + 1) / n + gap * i;
    return {x0, r.y, x1 - x0, r.h};
}

// Cell i of n equal cells stacked in `r`.
constexpr Rect columnCell(const Rect& r, int n, int i, int gap = 0) {
    if (n <= 0)
        return {};
    const int h = r.h - gap * (n - 1);
    const int y0 = r.y + h * i / n + gap * i, y1 = r.y + h * (i + 1) / n + gap * i;
    return {r.x, y0, r.w, y1 - y0};
}

// Cell (col, row) of a cols x rows grid.
constexpr Rect gridCell(const Rect& r, int cols, int rows, int col, int row, int gapX = 0, int gapY = 0) {
    return columnCell(rowCell(r, cols, col, gapX), rows, row, gapY);
}

// N rects of fixed width `w` at `pitch` pixels from x (a row of equally spaced controls).
template <std::size_t N> constexpr std::array<Rect, N> pitched(int x, int y, int w, int h, int pitch) {
    std::array<Rect, N> out{};
    for (std::size_t i = 0; i < N; ++i)
        out[i] = {x + static_cast<int>(i) * pitch, y, w, h};
    return out;
}

// Splits `r` at `at` pixels from its left (or top) edge; the rest after `gap`.
constexpr Rect leftPart(const Rect& r, int at) { return {r.x, r.y, at < r.w ? at : r.w, r.h}; }
constexpr Rect rightPart(const Rect& r, int at, int gap = 0) {
    const int x = r.x + at + gap;
    return {x, r.y, r.right() > x ? r.right() - x : 0, r.h};
}
constexpr Rect topPart(const Rect& r, int at) { return {r.x, r.y, r.w, at < r.h ? at : r.h}; }
constexpr Rect bottomPart(const Rect& r, int at, int gap = 0) {
    const int y = r.y + at + gap;
    return {r.x, y, r.w, r.bottom() > y ? r.bottom() - y : 0};
}

// A rect of size w x h centerd in `r`, or centerd on a point.
constexpr Rect centerd(const Rect& r, int w, int h) { return {r.x + (r.w - w) / 2, r.y + (r.h - h) / 2, w, h}; }
constexpr Rect around(int cx, int cy, int w, int h) { return {cx - w / 2, cy - h / 2, w, h}; }

// A cursor that hands out rects left to right (or top to bottom), for rows of mixed widths.
class Flow {
public:
    constexpr Flow(const Rect& area, int gap, bool vertical = false) : area_(area), gap_(gap), vertical_(vertical) {}
    constexpr Rect next(int size) {
        Rect r =
            vertical_ ? Rect{area_.x, area_.y + pos_, area_.w, size} : Rect{area_.x + pos_, area_.y, size, area_.h};
        pos_ += size + gap_;
        return r;
    }
    constexpr void skip(int pixels) { pos_ += pixels; }
    [[nodiscard]] constexpr int used() const { return pos_; }

private:
    Rect area_;
    int gap_;
    bool vertical_;
    int pos_ = 0;
};

} // namespace spux::inline v1::ui
