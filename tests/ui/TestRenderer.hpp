// A small CPU renderer for the spux_ui_core tests: draws axis-aligned and very plain (no anti-aliasing,
// circles and lines as filled pixels) into CpuLayers at scale 1, with clipping. Text is a box of 6 x 8
// pixels per character, so tests can see where it went. Not a flavor: the flavors' renderers are step 7.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "spux/ui/IRenderer.hpp"
#include "spux/ui/Layers.hpp"

namespace uitest {

using namespace spux::ui;

class TestRenderer final : public IRenderer {
public:
    TestRenderer(int w, int h, int scale = 1) : layers_(w, h, scale) {}

    [[nodiscard]] int width() const override { return layers_.width(); }
    [[nodiscard]] int height() const override { return layers_.height(); }
    LayerCache* layers() override { return &layers_; }
    CpuLayers& cpu() { return layers_; }
    int calls = 0; // drawing calls since construction

    void clear(Color c) override {
        ++calls;
        std::uint32_t* p = layers_.target();
        std::fill(p,
                  p + static_cast<std::size_t>(layers_.rowPixels()) *
                          static_cast<std::size_t>(layers_.height() * layers_.scale()),
                  c);
    }
    void pushClip(const Rect& r) override { clips_.push_back(clips_.empty() ? r : clips_.back().intersect(r)); }
    void popClip() override {
        if (!clips_.empty())
            clips_.pop_back();
    }
    void fillRect(const Rect& r, Color c) override {
        ++calls;
        fill(r, c);
    }
    void strokeRect(const Rect& r, Color c, float) override {
        ++calls;
        fill({r.x, r.y, r.w, 1}, c);
        fill({r.x, r.bottom() - 1, r.w, 1}, c);
        fill({r.x, r.y, 1, r.h}, c);
        fill({r.right() - 1, r.y, 1, r.h}, c);
    }
    void fillRoundRect(const Rect& r, float, Color c) override { fillRect(r, c); }
    void fillGradientRect(const Rect& r, Color top, Color) override { fillRect(r, top); }
    void drawLine(Point a, Point b, Color c, float) override {
        ++calls;
        const int n = static_cast<int>(std::max(std::fabs(b.x - a.x), std::fabs(b.y - a.y))) + 1;
        for (int i = 0; i <= n; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(n);
            plot(static_cast<int>(std::floor(a.x + (b.x - a.x) * t)),
                 static_cast<int>(std::floor(a.y + (b.y - a.y) * t)), c);
        }
    }
    void drawPolyline(const Point* pts, int count, Color c, float t) override {
        for (int i = 0; i + 1 < count; ++i)
            drawLine(pts[i], pts[i + 1], c, t);
    }
    void fillCircle(Point ce, float radius, Color c) override {
        ++calls;
        const int x0 = static_cast<int>(std::floor(ce.x - radius)), x1 = static_cast<int>(std::ceil(ce.x + radius));
        const int y0 = static_cast<int>(std::floor(ce.y - radius)), y1 = static_cast<int>(std::ceil(ce.y + radius));
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                const float dx = static_cast<float>(x) + 0.5f - ce.x, dy = static_cast<float>(y) + 0.5f - ce.y;
                if (dx * dx + dy * dy <= radius * radius)
                    plot(x, y, c);
            }
    }
    void strokeCircle(Point ce, float radius, Color c, float) override {
        ++calls;
        for (int i = 0; i < 64; ++i) {
            const float a = static_cast<float>(i) * 6.2831853f / 64.0f;
            plot(static_cast<int>(std::floor(ce.x + std::cos(a) * radius)),
                 static_cast<int>(std::floor(ce.y + std::sin(a) * radius)), c);
        }
    }
    void drawText(const char* text, int x, int y, Color c, int scale) override {
        ++calls;
        fill({x, y, textWidth(text, scale), textHeight(scale)}, c);
    }
    [[nodiscard]] int textWidth(const char* text, int scale) const override {
        return static_cast<int>(std::strlen(text)) * 6 * scale;
    }
    [[nodiscard]] int textHeight(int scale) const override { return 8 * scale; }

    // The resolved frame.
    std::vector<std::uint32_t> frame() {
        std::vector<std::uint32_t> out(static_cast<std::size_t>(width()) * static_cast<std::size_t>(height()));
        layers_.resolve(out.data(), {0, 0, width(), height()});
        return out;
    }

private:
    void plot(int x, int y, Color c) { fill({x, y, 1, 1}, c); }
    void fill(const Rect& r0, Color c) {
        Rect r = r0.intersect({0, 0, width(), height()});
        if (!clips_.empty())
            r = r.intersect(clips_.back());
        const int s = layers_.scale();
        std::uint32_t* p = layers_.target();
        const unsigned a = c >> 24;
        for (int y = r.y * s; y < r.bottom() * s; ++y)
            for (int x = r.x * s; x < r.right() * s; ++x) {
                std::uint32_t& d = p[static_cast<std::size_t>(y) * static_cast<std::size_t>(layers_.rowPixels()) +
                                     static_cast<std::size_t>(x)];
                if (a == 255) {
                    d = c;
                    continue;
                }
                auto ch = [&](int sh) {
                    const unsigned s0 = (d >> sh) & 255u, s1 = (c >> sh) & 255u;
                    return ((s0 * (255u - a) + s1 * a + 127u) / 255u) << sh;
                };
                d = 0xFF000000u | ch(16) | ch(8) | ch(0);
            }
    }

    CpuLayers layers_;
    std::vector<Rect> clips_;
};

} // namespace uitest
