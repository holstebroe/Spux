// Drawing helpers shared by the Modern style's painters and views (not installed, not part of the API).
#pragma once

#include "spux/ui/IRenderer.hpp"
#include "spux/ui/modern/FontAtlas.hpp"
#include "spux/ui/modern/ModernConfig.hpp"

namespace spux::inline v1::ui::modern {
namespace {

constexpr Color kWhite = 0xFFFFFFFFu;
constexpr float kCap = FontAtlas::kCapHeight;

// The top of a line of text of `px` that is vertically centered in a row.
float capY(float rowTop, float rowH, float px) { return rowTop + (rowH - kCap * px) * 0.5f; }

float f(int v) { return static_cast<float>(v); }

Color hairline(float a) { return withAlpha(kWhite, a); }

// Lights drawn after this call add to what is below and glow through the bloom (when the config lets them).
void lit(IRenderer& r, const ModernConfig& c, float emission) {
    if (c.effects.glow) {
        r.setBlend(Blend::Add);
        r.setEmission(emission * c.effects.emission);
    }
}
void unlit(IRenderer& r) {
    r.setEmission(1.0f);
    r.setBlend(Blend::Alpha);
}
// A halo's alpha under the config.
Color halo(const ModernConfig& c, Color col, float alpha) { return withAlpha(col, alpha * c.effects.glowStrength); }

} // namespace
} // namespace spux::inline v1::ui::modern
