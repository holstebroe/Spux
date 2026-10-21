// ADSR envelope with exponential segments, 0..1.
//
// Times are what they say (R4: derived from times, never from a fixed rate):
//   attack   the time to reach 1. The segment is an RC charge aimed at 1.05 and stopped at 1, the way an analog
//            envelope's capacitor is, so it starts steeply and flattens as it arrives.
//   decay    the time to fall by 60 dB (a thousandth) of the distance from 1 to the sustain level.
//   release  the time to fall by 60 dB from where the key was lifted; the voice is over at 1e-4 (-80 dB).
// A time is at least 1 ms; call prepare() before the first tick. gateOn() starts the attack from the current value (a
// retrigger does not jump to 0); gateOff() starts the release from wherever the envelope is, even in the attack.
// shaped(curve) blends the output toward an S-curve (smoothstep): 0 is linear, 1 the full S.
#pragma once

#include <algorithm>
#include <concepts>

#include "spux/core/Math.hpp"

namespace spux::inline v1::dsp {

template <std::floating_point T = float> class Adsr {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    static constexpr double kMinTime = 0.001;
    static constexpr T kSilence = T(1e-4);

    void prepare(double sampleRate) {
        fs_ = sampleRate;
        update();
    }
    void setAttack(double seconds) {
        attack_ = std::max(kMinTime, seconds);
        update();
    }
    void setDecay(double seconds) {
        decay_ = std::max(kMinTime, seconds);
        update();
    }
    void setRelease(double seconds) {
        release_ = std::max(kMinTime, seconds);
        update();
    }
    void setSustain(T level) { sustain_ = std::clamp(level, T(0), T(1)); }

    void reset() {
        stage_ = Stage::Idle;
        y_ = T(0);
        gate_ = false;
    }
    void gateOn() {
        gate_ = true;
        stage_ = Stage::Attack;
    }
    void gateOff() {
        gate_ = false;
        if (stage_ != Stage::Idle)
            stage_ = Stage::Release;
    }

    // One sample.
    T tick() {
        switch (stage_) {
        case Stage::Idle:
            y_ = T(0);
            break;
        case Stage::Attack:
            y_ += attackA_ * (kTarget - y_);
            if (y_ >= T(1)) {
                y_ = T(1);
                stage_ = Stage::Decay;
            }
            break;
        case Stage::Decay:
            y_ = sustain_ + decayA_ * (y_ - sustain_);
            if (y_ - sustain_ < kSilence && sustain_ - y_ < kSilence) {
                y_ = sustain_;
                stage_ = Stage::Sustain;
            }
            break;
        case Stage::Sustain:
            y_ = sustain_;
            break;
        case Stage::Release:
            y_ *= releaseA_;
            if (y_ < kSilence) {
                y_ = T(0);
                stage_ = Stage::Idle;
            }
            break;
        }
        return y_;
    }

    [[nodiscard]] T value() const { return y_; }
    [[nodiscard]] T shaped(T curve) const {
        const T v = std::clamp(y_, T(0), T(1));
        return v + std::clamp(curve, T(0), T(1)) * (v * v * (T(3) - T(2) * v) - v);
    }
    [[nodiscard]] Stage stage() const { return stage_; }
    [[nodiscard]] bool gate() const { return gate_; }
    // Sounding: the gate is down or the tail has not died away.
    [[nodiscard]] bool active() const { return stage_ != Stage::Idle; }

private:
    static constexpr T kTarget = T(1.05);
    static constexpr double kAttackRatio = 21.0;          // 1.05 / 0.05: the distance left when the charge reaches 1
    static constexpr double kSixtyDb = 6.907755278982137; // ln(1000)

    void update() {
        if (!(fs_ > 0.0))
            return;
        attackA_ = static_cast<T>(1.0 - math::expD(-math::logD(kAttackRatio) / (fs_ * attack_)));
        decayA_ = static_cast<T>(math::expD(-kSixtyDb / (fs_ * decay_)));
        releaseA_ = static_cast<T>(math::expD(-kSixtyDb / (fs_ * release_)));
    }

private:
    double fs_ = 0.0;
    double attack_ = 0.01, decay_ = 0.2, release_ = 0.3;
    T sustain_ = T(0.5);
    T attackA_ = T(0), decayA_ = T(0), releaseA_ = T(0);
    T y_ = T(0);
    Stage stage_ = Stage::Idle;
    bool gate_ = false;
};

} // namespace spux::inline v1::dsp
