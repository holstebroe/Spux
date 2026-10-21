#include "spux/ui/Icons.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spux::inline v1::ui {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;

constexpr Wave kWaves[kWaveCount] = {Wave::Sine,   Wave::Triangle, Wave::Saw,   Wave::SawDown,
                                     Wave::Square, Wave::Pulse,    Wave::Noise, Wave::SampleHold};

// A fixed pseudo-random level in -1..1 for step i, so the noise and sample-and-hold icons never change.
float level(int i) {
    std::uint32_t x = static_cast<std::uint32_t>(i) * 0x9E3779B1u + 0x7F4A7C15u;
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    x *= 0x297A2D39u;
    x ^= x >> 15;
    return static_cast<float>(x & 0xFFFFu) / 32767.5f - 1.0f;
}

void drawIcon(IRenderer& r, const Rect& area, Color color, float width, const void* data) {
    drawWave(r, *static_cast<const Wave*>(data), area, color, width);
}

} // namespace

void drawWave(IRenderer& r, Wave w, const Rect& area, Color color, float width) {
    if (area.empty())
        return;
    const float pad = width * 0.5f + 0.5f;
    const float x0 = static_cast<float>(area.x) + pad, x1 = static_cast<float>(area.right()) - pad;
    const float y0 = static_cast<float>(area.y) + pad, y1 = static_cast<float>(area.bottom()) - pad;
    if (x1 <= x0 || y1 <= y0)
        return;
    const float mid = 0.5f * (y0 + y1);
    // u in 0..1 across, v in -1..1 (up is positive)
    auto pt = [&](float u, float v) { return Point{x0 + (x1 - x0) * u, mid - 0.5f * (y1 - y0) * v}; };
    Point p[48];
    int n = 0;
    auto add = [&](float u, float v) {
        if (n < 48)
            p[n++] = pt(u, v);
    };
    switch (w) {
    case Wave::Sine:
        for (int i = 0; i <= 32; ++i)
            add(static_cast<float>(i) / 32.0f, std::sin(kTwoPi * static_cast<float>(i) / 32.0f));
        break;
    case Wave::Triangle:
        add(0.0f, 0.0f), add(0.25f, 1.0f), add(0.75f, -1.0f), add(1.0f, 0.0f);
        break;
    case Wave::Saw:
        add(0.0f, -1.0f), add(0.5f, 1.0f), add(0.5f, -1.0f), add(1.0f, 1.0f);
        break;
    case Wave::SawDown:
        add(0.0f, 1.0f), add(0.5f, -1.0f), add(0.5f, 1.0f), add(1.0f, -1.0f);
        break;
    case Wave::Square:
        add(0.0f, -1.0f), add(0.0f, 1.0f), add(0.5f, 1.0f), add(0.5f, -1.0f), add(1.0f, -1.0f);
        break;
    case Wave::Pulse:
        add(0.0f, -1.0f), add(0.2f, -1.0f), add(0.2f, 1.0f), add(0.4f, 1.0f), add(0.4f, -1.0f), add(1.0f, -1.0f);
        break;
    case Wave::Noise:
        for (int i = 0; i <= 16; ++i)
            add(static_cast<float>(i) / 16.0f, level(i));
        break;
    case Wave::SampleHold:
        for (int i = 0; i < 5; ++i) {
            const float v = level(i + 40) * 0.9f;
            add(static_cast<float>(i) / 5.0f, v);
            add(static_cast<float>(i + 1) / 5.0f, v);
        }
        break;
    }
    r.drawPolyline(p, n, color, width);
}

Icon waveIcon(Wave w) { return {drawIcon, &kWaves[static_cast<int>(w)]}; }

} // namespace spux::inline v1::ui
