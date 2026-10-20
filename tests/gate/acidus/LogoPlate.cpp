// Copied from Acidus src/gui/GuiWindow.cpp (see LogoPlate.hpp); only the names of the members changed.
#include "LogoPlate.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "clap/AcidusClap.hpp"

namespace acidus {

void LogoPlate::draw(Graphics& g, unsigned parts) {
    const int startX = startX_, startY = startY_;
    // Helper lambda to draw letter primitives for ACIDUS
    auto renderLetters = [&](Graphics& gfx, int x, int y, uint32_t color) {
        // A
        int aX = x;
        gfx.drawRect(aX, y + 6, 5, 32, color);
        gfx.drawRect(aX + 15, y + 6, 5, 32, color);
        gfx.drawRect(aX + 4, y, 12, 6, color);
        gfx.drawRect(aX + 4, y + 18, 12, 5, color);

        // C
        int cX = x + 24;
        gfx.drawRect(cX, y, 5, 38, color);
        gfx.drawRect(cX, y, 18, 6, color);
        gfx.drawRect(cX, y + 32, 18, 6, color);

        // I
        int iX = x + 46;
        gfx.drawRect(iX, y, 10, 5, color);
        gfx.drawRect(iX + 3, y, 4, 38, color);
        gfx.drawRect(iX, y + 33, 10, 5, color);

        // D
        int dX = x + 60;
        gfx.drawRect(dX, y, 5, 38, color);
        gfx.drawRect(dX, y, 15, 6, color);
        gfx.drawRect(dX, y + 32, 15, 6, color);
        gfx.drawRect(dX + 15, y + 5, 5, 28, color);

        // U
        int uX = x + 84;
        gfx.drawRect(uX, y, 5, 34, color);
        gfx.drawRect(uX + 15, y, 5, 34, color);
        gfx.drawRect(uX, y + 32, 20, 6, color);

        // S
        int sX = x + 108;
        gfx.drawRect(sX, y, 20, 6, color);
        gfx.drawRect(sX, y, 5, 19, color);
        gfx.drawRect(sX, y + 16, 20, 6, color);
        gfx.drawRect(sX + 15, y + 18, 5, 17, color);
        gfx.drawRect(sX, y + 32, 20, 6, color);
    };

    // Glow layers (outer dim glow, inner medium glow, core acid green, core bright highlight)
    uint32_t dimGlow = 0xFF0A400F;    // Faint dark green outer halo
    uint32_t medGlow = 0xFF1B8224;    // Medium green inner halo
    uint32_t acidGreen = 0xFF39FF14;  // Core Acid Green
    uint32_t brightCore = 0xFFBFFF80; // Bright yellow-green center highlight

    // 0. Backing plate: a dark LCD-style badge the glow can pop against,
    // instead of sitting directly on the brushed silver panel.
    const int logoW = 128; // A..S span, see renderLetters offsets above
    const int logoH = 38;
    const int subtitleGap = 8;
    const int subtitleH = 7; // font_ glyph height at scale 1
    int plateX = startX - 16;
    int plateY = startY - 14;
    int plateW = logoW + 32;
    const int smileyRow = 20; // room for the acid smiley under the tagline
    const int presetRow = 14; // calibration preset label under the smiley
    int plateH = logoH + subtitleGap + subtitleH + 26 + smileyRow + presetRow;
    plateX_ = plateX;
    plateY_ = plateY;
    plateW_ = plateW;
    plateH_ = plateH;

    const char* subtitle = "ANALOG BASS SYNTH";
    int subtitleW = font_.getTextWidth(subtitle, 1);
    int subtitleX = startX + (logoW - subtitleW) / 2;
    int subtitleY = startY + logoH + subtitleGap;

    if (parts & kPlate) {
        g.fillRect(plateX + 3, plateY + 4, plateW, plateH, 0x50000000);
        g.fillRect(plateX, plateY, plateW, plateH, 0xFF131517);
        g.drawRect(plateX - 2, plateY - 2, plateW + 4, plateH + 4, 0xFF4A4E52);
        g.drawRect(plateX, plateY, plateW, plateH, 0xFF040506);
        g.drawLine(plateX + 2, plateY + 2, plateX + plateW - 3, plateY + 2, 0xFF2C3030, 1);
    }

    // (The bubbles are drawn here, between the plate and the lettering; see
    // renderFrame.)

    if (parts & kForeground) {
        // Small corner screws for a hardware badge feel.
        for (int sx = 0; sx < 2; ++sx) {
            for (int sy = 0; sy < 2; ++sy) {
                int scx = plateX + 7 + sx * (plateW - 14);
                int scy = plateY + 7 + sy * (plateH - 14);
                g.fillCircle(scx, scy, 3, 0xFF3A3E42);
                g.drawCircle(scx, scy, 3, 0xFF08090A, 1);
                g.drawLine(scx - 2, scy, scx + 2, scy, 0xFF1A1C1E, 1);
            }
        }
    }

    if (parts & kGlow) {
        // 1. Outer halo (pass offsets -2 to +2)
        for (int dx = -2; dx <= 2; ++dx) {
            for (int dy = -2; dy <= 2; ++dy) {
                if (dx == 0 && dy == 0)
                    continue;
                renderLetters(g, startX + dx, startY + dy, dimGlow);
            }
        }

        // 2. Inner halo (pass offsets -1 to +1)
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0)
                    continue;
                renderLetters(g, startX + dx, startY + dy, medGlow);
            }
        }
    }

    if (parts & kForeground) {
        // 3. Core Acid Green
        renderLetters(g, startX, startY, acidGreen);

        // 4. Subtle inner highlight line
        renderLetters(g, startX + 1, startY + 1, brightCore);

        // 5. Tagline underneath the lockup, in the panel's own pixel font.
        g.drawText(font_, subtitle, subtitleX, subtitleY, 0xFF1B8224, 1);
    }

    if (!(parts & kDynamic))
        return;

    // 6. The acid smiley.
    drawSmiley(g, startX + logoW / 2, subtitleY + subtitleH + 13);

    // 7. Calibration preset label: a small recessed LCD window. A star
    // means a calibration parameter was changed after loading the preset.
    // Click to load the next preset.
    // A status message (export / import outcome) replaces it for a moment.
    char label[48];
    if (!status_.empty() && std::chrono::steady_clock::now() < statusUntil_) {
        std::snprintf(label, sizeof(label), "%s", status_.c_str());
    } else if (plugin_) {
        std::snprintf(label, sizeof(label), "%s%s", plugin_->calibrationPresetName().c_str(),
                      plugin_->isCalibrationModified() ? "*" : "");
    } else {
        std::snprintf(label, sizeof(label), "%s", "X0X");
    }
    const int labelTextW = font_.getTextWidth(label, 1);
    labelW_ = std::max(labelTextW, font_.getTextWidth("HELL FISH*", 1)) + 12;
    labelH_ = 11;
    labelX_ = startX + (logoW - labelW_) / 2;
    labelY_ = subtitleY + subtitleH + 27;
    g.fillRect(labelX_, labelY_, labelW_, labelH_, 0xFF070809);
    g.drawRect(labelX_, labelY_, labelW_, labelH_, 0xFF2C3030);
    g.drawLine(labelX_ + 1, labelY_ + 1, labelX_ + labelW_ - 2, labelY_ + 1, 0xFF000000, 1);
    g.drawText(font_, label, labelX_ + (labelW_ - labelTextW) / 2, labelY_ + 2, 0xFF39FF14, 1);
}

void LogoPlate::advance(double dt, double cutoffNorm) {
    if (!(dt > 0.0))
        return;             // also rejects NaN
    dt = std::min(dt, 0.1); // don't burst after a stall
    animTime_ += dt;
    eyeFlash_ = std::max(0.0, eyeFlash_ - dt);
    if (plateW_ <= 0 || plateH_ <= 0)
        return;

    // Bubble rate follows the Cutoff knob: a lazy trickle when closed, a
    // fizz when wide open.
    const double cutoff = std::isfinite(cutoffNorm) ? std::min(std::max(cutoffNorm, 0.0), 1.0) : 0.5;
    const double rate = 0.5 + 12.0 * std::pow(cutoff, 1.5); // bubbles per second
    spawnAccum_ += rate * dt;

    std::uniform_real_distribution<double> uni(0.0, 1.0);
    while (spawnAccum_ >= 1.0 && bubbles_.size() < 256) {
        spawnAccum_ -= 1.0;
        Bubble b;
        b.r = 1.0 + 2.2 * uni(rng_);
        b.x = 6.0 + (plateW_ - 12.0) * uni(rng_);
        b.y = plateH_ - 4.0;
        b.speed = 10.0 + 18.0 * uni(rng_) + 6.0 / b.r; // small ones rise faster
        b.phase = 6.283 * uni(rng_);
        bubbles_.push_back(b);
    }
    spawnAccum_ = std::min(spawnAccum_, 1.0);
    for (auto& b : bubbles_) {
        b.y -= b.speed * dt;
    }
    bubbles_.erase(std::remove_if(bubbles_.begin(), bubbles_.end(), [](const Bubble& b) { return b.y < 4.0 + b.r; }),
                   bubbles_.end());
}

void LogoPlate::drawBubbles(Graphics& g) {
    for (const auto& b : bubbles_) {
        double wobble = 1.6 * std::sin(animTime_ * 3.0 + b.phase + b.y * 0.08);
        int cx = plateX_ + static_cast<int>(std::lround(b.x + wobble));
        int cy = plateY_ + static_cast<int>(std::lround(b.y));
        int r = std::max(1, static_cast<int>(std::lround(b.r)));
        // Fade in near the bottom and out near the top of the plate.
        double t = std::min(1.0, std::min((plateH_ - b.y) / 12.0, (b.y - 4.0) / 14.0));
        uint32_t a = static_cast<uint32_t>(std::max(0.0, t) * 0x90);
        g.fillCircle(cx, cy, r, (a / 3) << 24 | 0x0039FF14);
        g.drawCircle(cx, cy, r, a << 24 | 0x0039FF14, 1);
        if (r >= 2)
            g.fillCircle(cx - 1, cy - 1, 1, (a << 24) | 0x00CFFFB0);
    }
}

void LogoPlate::drawSmiley(Graphics& g, int cx, int cy) {
    const int r = 7;
    g.fillCircle(cx, cy, r + 2, 0x40FFD21E); // soft glow on the dark plate
    g.fillCircle(cx, cy, r, 0xFFFFD21E);
    g.drawCircle(cx, cy, r, 0xFF5A4300, 1);
    if (eyeFlash_ > 0.0) { // eyes, wide on an accent
        g.fillRect(cx - 4, cy - 4, 3, 4, 0xFF101010);
        g.fillRect(cx + 2, cy - 4, 3, 4, 0xFF101010);
    } else {
        g.fillRect(cx - 3, cy - 3, 2, 3, 0xFF101010);
        g.fillRect(cx + 2, cy - 3, 2, 3, 0xFF101010);
    }
    for (int dx = -4; dx <= 4; ++dx) { // smile
        int dy = static_cast<int>(std::lround(1.0 + 0.16 * (16 - dx * dx) * 0.28));
        g.fillRect(cx + dx, cy + 1 + dy, 1, 1, 0xFF101010);
    }
    g.fillRect(cx - 5, cy + 1, 1, 1, 0xFF101010); // mouth corners
    g.fillRect(cx + 5, cy + 1, 1, 1, 0xFF101010);
}

double LogoPlate::glowPulse() const {
    // One slow breath per bar: brightest on the downbeat. Synced to the
    // host's bar while its transport plays, else a free-running 2 s cycle.
    double phase = 0.0;
    if (!plugin_ || !plugin_->transportBarPhase(phase)) {
        phase = std::fmod(animTime_ / 2.0, 1.0);
    }
    const double breath = 0.5 + 0.5 * std::cos(2.0 * 3.14159265358979323846 * phase);
    return 0.7 + 0.3 * breath;
}

void LogoPlate::drawAnimated(Graphics& g, int width, int height) {
    const int hW = width * 2, hH = height * 2;
    if (!overlayValid_) {
        overlay_.assign(static_cast<size_t>(hW) * hH, 0u);
        Graphics og(overlay_.data(), width, height, 2);
        draw(og, kForeground);
        glow_.assign(static_cast<size_t>(hW) * hH, 0u);
        Graphics gg(glow_.data(), width, height, 2);
        draw(gg, kGlow);
        overlayValid_ = true;
    }
    drawBubbles(g);
    const int ox0 = std::max(0, 2 * (plateX_ - 4)), oy0 = std::max(0, 2 * (plateY_ - 4));
    const int ox1 = std::min(hW, 2 * (plateX_ + plateW_ + 6)), oy1 = std::min(hH, 2 * (plateY_ + plateH_ + 6));
    const double pulse = glowPulse();
    const uint32_t glowAlpha =
        static_cast<uint32_t>(std::min(std::max(std::isfinite(pulse) ? 255.0 * pulse : 255.0, 0.0), 255.0));
    for (int y = oy0; y < oy1; ++y) {
        for (int x = ox0; x < ox1; ++x) {
            const size_t i = static_cast<size_t>(y) * hW + x;
            const uint32_t gl = glow_[i];
            if (gl >> 24)
                g.blendPixel(x, y, (glowAlpha << 24) | (gl & 0x00FFFFFFu));
            const uint32_t o = overlay_[i];
            if (o >> 24)
                g.blendPixel(x, y, o);
        }
    }
    draw(g, kDynamic);
}

void LogoPlate::showStatus(const std::string& text) {
    status_ = text;
    statusUntil_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
}

std::string LogoPlate::status() const {
    return std::chrono::steady_clock::now() < statusUntil_ ? status_ : std::string();
}

std::uint64_t LogoPlate::key() const {
    std::uint64_t h = 1469598103934665603ull;
    auto mixIn = [&h](const void* p, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) {
            h ^= static_cast<const unsigned char*>(p)[i];
            h *= 1099511628211ull;
        }
    };
    const double pulse = glowPulse();
    const bool eyes = eyeFlash_ > 0.0;
    mixIn(&animTime_, sizeof animTime_);
    mixIn(&pulse, sizeof pulse);
    mixIn(&eyes, sizeof eyes);
    const std::string label = status().empty() && plugin_
                                  ? plugin_->calibrationPresetName() + (plugin_->isCalibrationModified() ? "*" : "")
                                  : status();
    mixIn(label.data(), label.size());
    return h;
}

} // namespace acidus
