// Noise sources driven by PCG32 (R3). The generator is seeded by the
// plugin (Pcg32::derive from the patch seed, voice and note), never from time.
//
//   WhiteNoise  uniform in [-1, 1) (RMS 0.577).
//   BrownNoise  white noise through a leaky integrator (-6 dB per octave above its corner), scaled so its RMS
//               matches the white noise it is made of.
//   CrackleNoise  random clicks (a Poisson process at a given rate, random sign and size) ringing out over a short
//               decay: vinyl and Geiger-counter crackle.
//   PinkNoise   Paul Kellet's economy pink filter on the white noise (-3 dB per octave; the poles are
//               fixed per sample, so the band where the slope holds scales with the rate: the test checks
//               100 Hz to 5 kHz at 48 kHz), scaled by 0.25 to about 0.43 RMS.
#pragma once

#include <cmath>
#include <concepts>

#include "spux/core/Math.hpp"
#include "spux/core/Pcg32.hpp"
#include "spux/core/Smoothing.hpp"

namespace spux::inline v1::dsp {

template <std::floating_point T = float> class WhiteNoise {
public:
    void seed(const Pcg32& rng) { rng_ = rng; }
    T tick() { return static_cast<T>(rng_.nextBipolar()); }

private:
    Pcg32 rng_;
};

template <std::floating_point T = float> class PinkNoise {
public:
    // Seeds the generator and clears the filter.
    void seed(const Pcg32& rng) {
        rng_ = rng;
        b0_ = b1_ = b2_ = T(0);
    }
    T tick() {
        const auto w = static_cast<T>(rng_.nextBipolar());
        b0_ = T(0.99765f) * b0_ + w * T(0.0990460f);
        b1_ = T(0.96300f) * b1_ + w * T(0.2965164f);
        b2_ = T(0.57000f) * b2_ + w * T(1.0526913f);
        return (b0_ + b1_ + b2_ + w * T(0.1848f)) * T(0.25f);
    }

private:
    Pcg32 rng_;
    T b0_ = T(0), b1_ = T(0), b2_ = T(0);
};

template <std::floating_point T = float> class BrownNoise {
public:
    // The leak from the corner frequency (Hz): below it the spectrum flattens; default 140 Hz.
    void prepare(double sampleRate, double cornerHz = 140.0) {
        const double a = math::expD(-math::kTwoPi * cornerHz / sampleRate);
        a_ = static_cast<T>(a);
        // unit gain at the RMS of the input: sqrt(1 - a^2)
        gain_ = static_cast<T>(std::sqrt(1.0 - a * a));
    }
    void seed(const Pcg32& rng) {
        rng_ = rng;
        y_ = T(0);
    }
    T tick() {
        y_ = a_ * y_ + gain_ * static_cast<T>(rng_.nextBipolar());
        return y_;
    }

private:
    Pcg32 rng_;
    T a_ = T(0.98), gain_ = T(0.2), y_ = T(0);
};

template <std::floating_point T = float> class CrackleNoise {
public:
    void prepare(double sampleRate) {
        fs_ = sampleRate;
        update();
    }
    // Clicks per second.
    void setRate(double hz) {
        rate_ = hz;
        update();
    }
    // How long a click rings, as a time constant in seconds (default 0.27 ms).
    void setDecay(double seconds) {
        decay_ = seconds;
        update();
    }
    void seed(const Pcg32& rng) {
        rng_ = rng;
        y_ = T(0);
    }
    T tick() {
        T spike = T(0);
        if (rng_.nextFloat() < prob_) {
            const T size = T(0.5) + T(0.5) * static_cast<T>(rng_.nextFloat());
            spike = (rng_.next() & 1u) ? size : -size;
        }
        y_ = y_ * keep_ + spike;
        return y_;
    }

private:
    void update() {
        prob_ = fs_ > 0.0 ? static_cast<float>(rate_ / fs_) : 0.0f;
        keep_ = static_cast<T>(1.0 - onePoleCoeff(decay_, fs_));
    }
    Pcg32 rng_;
    double fs_ = 0.0, rate_ = 100.0, decay_ = 0.00027;
    float prob_ = 0.0f;
    T keep_ = T(0.92), y_ = T(0);
};

} // namespace spux::inline v1::dsp
