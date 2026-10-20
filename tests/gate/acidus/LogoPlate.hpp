// Acceptance gate, Acidus port: the animated logo plate (bubbles, glowing lettering, the smiley and the
// calibration label). The drawing and the animation are copied from Acidus `src/gui/GuiWindow.cpp`
// (drawAcidusTitle, advanceAnimation, drawBubbles, drawSmiley, glowPulse; MIT, same author) so the
// immediate-mode port draws the same pixels. It is drawing, not layout: the gate's line count leaves it
// out on both sides.
#pragma once

#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "Font.hpp"
#include "Graphics.hpp"

namespace acidus {

class AcidusClap;

class LogoPlate {
public:
    static constexpr unsigned kPlate = 1;      // box and shadow (background)
    static constexpr unsigned kForeground = 2; // screws, lettering, tagline (cached overlay)
    static constexpr unsigned kGlow = 8;       // the lettering's halo (pulsing overlay)
    static constexpr unsigned kDynamic = 4;    // smiley and calibration label (every frame)
    static constexpr double kEyeFlashSec = 0.18;

    LogoPlate(AcidusClap* plugin, const Font& font, int x, int y)
        : plugin_(plugin), font_(font), startX_(x), startY_(y) {}

    void draw(Graphics& g, unsigned parts);
    // The animated region: bubbles under the cached lettering, then the dynamic parts.
    void drawAnimated(Graphics& g, int width, int height);
    void advance(double dt, double cutoffNorm);
    void flashEyes() { eyeFlash_ = kEyeFlashSec; }
    bool eyesFlashing() const { return eyeFlash_ > 0.0; }
    void showStatus(const std::string& text);
    std::string status() const;

    // Valid after the first draw.
    void plateRect(int& x, int& y, int& w, int& h) const {
        x = plateX_;
        y = plateY_;
        w = plateW_;
        h = plateH_;
    }
    void labelRect(int& x, int& y, int& w, int& h) const {
        x = labelX_;
        y = labelY_;
        w = labelW_;
        h = labelH_;
    }
    // A key that changes whenever the animated region looks different.
    std::uint64_t key() const;

private:
    struct Bubble {
        double x, y, r, speed, phase;
    };
    void drawBubbles(Graphics& g);
    void drawSmiley(Graphics& g, int cx, int cy);
    double glowPulse() const;

    AcidusClap* plugin_;
    Font font_;
    int startX_, startY_;
    int plateX_ = 0, plateY_ = 0, plateW_ = 0, plateH_ = 0;
    int labelX_ = 0, labelY_ = 0, labelW_ = 0, labelH_ = 0;
    std::vector<Bubble> bubbles_;
    std::mt19937 rng_{0xAC1D};
    double spawnAccum_ = 0.0, animTime_ = 0.0, eyeFlash_ = 0.0;
    std::vector<std::uint32_t> overlay_, glow_;
    bool overlayValid_ = false;
    std::string status_;
    std::chrono::steady_clock::time_point statusUntil_{};
};

} // namespace acidus
