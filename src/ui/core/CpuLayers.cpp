#include "spux/ui/Layers.hpp"

#include <algorithm>
#include <cstring>

namespace spux::inline v1::ui {

namespace {
constexpr std::size_t kMaxDirty = 48; // beyond this the list collapses into its bounding box
}

void CpuLayers::resize(int width, int height, int scale) {
    w_ = std::max(0, width);
    h_ = std::max(0, height);
    s_ = std::max(1, scale);
    const std::size_t n =
        static_cast<std::size_t>(w_) * static_cast<std::size_t>(h_) * static_cast<std::size_t>(s_ * s_);
    for (auto& l : layers_)
        l.assign(n, 0xFF000000u);
    dirty_.clear();
    bounds_ = {};
    composited_ = displayValid_ = false;
    markDirty({0, 0, w_, h_});
}

std::uint32_t* CpuLayers::buffer(Layer layer) { return layers_[static_cast<int>(layer)].data(); }
const std::uint32_t* CpuLayers::buffer(Layer layer) const { return layers_[static_cast<int>(layer)].data(); }

void CpuLayers::copy(Layer to, Layer from, const Rect& r) {
    const Rect c = clip(r);
    if (c.empty())
        return;
    const int row = rowPixels();
    std::uint32_t* dst = buffer(to);
    const std::uint32_t* src = buffer(from);
    const std::size_t n = static_cast<std::size_t>(c.w * s_) * sizeof(std::uint32_t);
    for (int y = c.y * s_; y < c.bottom() * s_; ++y) {
        const std::size_t o =
            static_cast<std::size_t>(y) * static_cast<std::size_t>(row) + static_cast<std::size_t>(c.x * s_);
        std::memcpy(dst + o, src + o, n);
    }
}

void CpuLayers::restore(const Rect& r) {
    copy(Layer::Widgets, Layer::Background, r);
    markDirty(r);
}

void CpuLayers::markDirty(const Rect& r) {
    const Rect c = clip(r);
    if (c.empty())
        return;
    bounds_ = bounds_.unite(c);
    for (const Rect& d : dirty_)
        if (d.intersect(c) == c)
            return; // already covered
    if (dirty_.size() >= kMaxDirty) {
        Rect all = c;
        for (const Rect& d : dirty_)
            all = all.unite(d);
        dirty_.assign(1, all);
        return;
    }
    dirty_.push_back(c);
}

void CpuLayers::composite() {
    if (!displayValid_) {
        copy(Layer::Display, Layer::Widgets, {0, 0, w_, h_});
        displayValid_ = true;
    } else {
        for (const Rect& d : dirty_)
            copy(Layer::Display, Layer::Widgets, d);
    }
    dirty_.clear();
    composited_ = true;
}

void CpuLayers::clearDirty() {
    if (!composited_)
        displayValid_ = false; // widgets changed without the display layer following
    composited_ = false;
    dirty_.clear();
    bounds_ = {};
}

void CpuLayers::resolve(std::uint32_t* out, const Rect& area) const {
    const Rect c = clip(area);
    if (c.empty())
        return;
    const std::uint32_t* src = buffer(composited_ ? Layer::Display : Layer::Widgets);
    const int row = rowPixels();
    if (s_ == 1) {
        for (int y = c.y; y < c.bottom(); ++y)
            for (int x = c.x; x < c.right(); ++x)
                out[static_cast<std::size_t>(y) * static_cast<std::size_t>(w_) + static_cast<std::size_t>(x)] =
                    0xFF000000u |
                    src[static_cast<std::size_t>(y) * static_cast<std::size_t>(row) + static_cast<std::size_t>(x)];
        return;
    }
    if (s_ == 2) { // the Classic case, unrolled: the rounded mean of each 2 x 2 block
        for (int y = c.y; y < c.bottom(); ++y) {
            const std::uint32_t* r0 = src + static_cast<std::size_t>(2 * y) * static_cast<std::size_t>(row) +
                                      static_cast<std::size_t>(2 * c.x);
            const std::uint32_t* r1 = r0 + row;
            std::uint32_t* o =
                out + static_cast<std::size_t>(y) * static_cast<std::size_t>(w_) + static_cast<std::size_t>(c.x);
            for (int x = 0; x < c.w; ++x, r0 += 2, r1 += 2) {
                const std::uint32_t p00 = r0[0], p01 = r0[1], p10 = r1[0], p11 = r1[1];
                // red and blue in one word, green in another: no channel overflows its 10 bits
                const std::uint32_t rb =
                    (p00 & 0xFF00FFu) + (p01 & 0xFF00FFu) + (p10 & 0xFF00FFu) + (p11 & 0xFF00FFu) + 0x020002u;
                const std::uint32_t g = (p00 & 0xFF00u) + (p01 & 0xFF00u) + (p10 & 0xFF00u) + (p11 & 0xFF00u) + 0x200u;
                o[x] = 0xFF000000u | ((rb >> 2) & 0xFF00FFu) | ((g >> 2) & 0xFF00u);
            }
        }
        return;
    }
    const unsigned n = static_cast<unsigned>(s_ * s_), half = n / 2;
    for (int y = c.y; y < c.bottom(); ++y) {
        for (int x = c.x; x < c.right(); ++x) {
            unsigned r = 0, g = 0, b = 0;
            for (int sy = 0; sy < s_; ++sy) {
                const std::uint32_t* p = src + static_cast<std::size_t>(y * s_ + sy) * static_cast<std::size_t>(row) +
                                         static_cast<std::size_t>(x * s_);
                for (int sx = 0; sx < s_; ++sx) {
                    r += (p[sx] >> 16) & 0xFFu;
                    g += (p[sx] >> 8) & 0xFFu;
                    b += p[sx] & 0xFFu;
                }
            }
            out[static_cast<std::size_t>(y) * static_cast<std::size_t>(w_) + static_cast<std::size_t>(x)] =
                0xFF000000u | (((r + half) / n) << 16) | (((g + half) / n) << 8) | ((b + half) / n);
        }
    }
}

} // namespace spux::inline v1::ui
