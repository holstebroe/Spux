// Noise sources driven by PCG32 (R3). The generator is seeded by the
// plugin (Pcg32::derive from the patch seed, voice and note), never from time.
//
//   WhiteNoise  uniform in [-1, 1) (RMS 0.577).
//   PinkNoise   Paul Kellet's economy pink filter on the white noise (-3 dB per octave; the poles are
//               fixed per sample, so the band where the slope holds scales with the rate: the test checks
//               100 Hz to 5 kHz at 48 kHz), scaled by 0.25 to about 0.43 RMS.
#pragma once

#include <concepts>

#include "spux/core/Pcg32.hpp"

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

} // namespace spux::inline v1::dsp
