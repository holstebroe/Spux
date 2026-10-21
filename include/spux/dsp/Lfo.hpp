// Low-frequency oscillator over the common waves (spux/core/Wave.hpp), bipolar -1..1, with no depth: the amount
// of a modulation belongs to the route that uses it (ModMatrix).
//
// tick() returns the value at the current phase and then advances, so the first sample after reset(phase) is
// that phase's value. The random waves draw from the generator given to seed() (PCG32, R3): Noise joins a new
// random value each period to the last with a smoothstep, SampleHold holds it. Rate is in Hz, any value above 0;
// a plugin clamps it to what its knob means.
#pragma once

#include <concepts>

#include "spux/core/Math.hpp"
#include "spux/core/Pcg32.hpp"
#include "spux/core/Wave.hpp"

namespace spux::inline v1::dsp {

template <std::floating_point T = float> class Lfo {
public:
    void prepare(double sampleRate) {
        fs_ = sampleRate;
        update();
    }
    void setRate(double hz) {
        rate_ = hz;
        update();
    }
    void setWave(Wave w) { wave_ = w; }
    [[nodiscard]] Wave wave() const { return wave_; }

    // Seeds the random waves and starts the first segment.
    void seed(const Pcg32& rng) {
        rng_ = rng;
        from_ = to_ = rng_.nextBipolar();
        to_ = rng_.nextBipolar();
    }
    // Back to a phase in [0, 1); the random waves keep their segment.
    void reset(double phase = 0.0) { phase_ = phase - math::floorD(phase); }

    T tick() {
        const T v = valueAt(phase_);
        phase_ += inc_;
        if (phase_ >= 1.0) {
            phase_ -= math::floorD(phase_);
            from_ = to_;
            to_ = rng_.nextBipolar();
        }
        return v;
    }
    [[nodiscard]] double phase() const { return phase_; }

private:
    void update() { inc_ = fs_ > 0.0 ? rate_ / fs_ : 0.0; }

    [[nodiscard]] T valueAt(double p) const {
        switch (wave_) {
        case Wave::Sine:
            return static_cast<T>(math::sinTurns(p));
        case Wave::Triangle:
            return static_cast<T>(p < 0.25 ? 4.0 * p : p < 0.75 ? 2.0 - 4.0 * p : 4.0 * p - 4.0);
        case Wave::Saw:
            return static_cast<T>(2.0 * p - 1.0);
        case Wave::SawDown:
            return static_cast<T>(1.0 - 2.0 * p);
        case Wave::Square:
            return p < 0.5 ? T(1) : T(-1);
        case Wave::Pulse:
            return p < 0.25 ? T(1) : T(-1);
        case Wave::Noise: {
            const T s = static_cast<T>(p * p * (3.0 - 2.0 * p));
            return from_ + (to_ - from_) * s;
        }
        case Wave::SampleHold:
            return from_;
        }
        return T(0);
    }

    double fs_ = 0.0, rate_ = 1.0, inc_ = 0.0;
    double phase_ = 0.0;
    Wave wave_ = Wave::Sine;
    T from_ = T(0), to_ = T(0);
    Pcg32 rng_;
};

} // namespace spux::inline v1::dsp
