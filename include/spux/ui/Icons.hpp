// Icons: small drawings a widget can show in place of a label (a button, the steps of a selector). An Icon is a
// plain function and a pointer to what it draws, so a plugin writes its own as one function and passes it where
// the built-in ones go; drawing goes through IRenderer only, so every flavor shows every icon.
//
//   void drawGear(IRenderer& r, const Rect& area, Color c, float width, const void* data) { ... }
//   g.button("settings", rect, "", {.icon = {drawGear}});
//
// The built-in set is the waveforms nearly every oscillator wants (Wave): waveIcon(Wave::Saw) is the Icon, and
// drawWave draws one directly (a view, a background).
#pragma once

#include "spux/ui/IRenderer.hpp"

namespace spux::inline v1::ui {

struct Icon {
    // Draws inside `area` with the given color and stroke width in logical pixels; `data` is what the icon
    // was made with (a pointer the plugin owns for as long as the icon is used).
    using Draw = void (*)(IRenderer& r, const Rect& area, Color color, float width, const void* data);
    Draw draw = nullptr;
    const void* data = nullptr;

    explicit operator bool() const { return draw != nullptr; }
    void operator()(IRenderer& r, const Rect& area, Color color, float width = 1.5f) const {
        if (draw)
            draw(r, area, color, width, data);
    }
    friend bool operator==(const Icon&, const Icon&) = default;
};

// One period (or two teeth, steps) of the common oscillator shapes.
enum class Wave { Sine, Triangle, Saw, SawDown, Square, Pulse, Noise, SampleHold };
inline constexpr int kWaveCount = 8;

[[nodiscard]] const char* waveName(Wave w);
// The wave drawn as a line `width` thick, centered in `area` with room for the stroke.
void drawWave(IRenderer& r, Wave w, const Rect& area, Color color, float width = 1.5f);
// The built-in icon of a wave.
[[nodiscard]] Icon waveIcon(Wave w);

} // namespace spux::inline v1::ui
