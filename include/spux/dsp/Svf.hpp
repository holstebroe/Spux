// Linear state-variable filter, trapezoidal (TPT; Simper's form): low-pass, band-pass and high-pass from
// one update, stable under coefficient changes every sample. Generic only: a saturating SVF (an OTA
// SVF) is character code and stays in its plugin.
//
// Law: g = tan(pi fc / fs), k = 1 / Q (damping, 2 = no resonance, 0 = self-oscillation);
//   a1 = 1 / (1 + g (g + k)); v3 = x - ic2; v1 = a1 (ic1 + g v3); v2 = ic2 + g v1;
//   ic1' = 2 v1 - ic1; ic2' = 2 v2 - ic2; low = v2, band = v1, high = x - k v1 - v2.
// The cutoff is clamped to [1e-6, 0.49] of the sample rate.
#pragma once

#include <concepts>

#include "spux/core/Math.hpp"

namespace spux::inline v1::dsp {

template <std::floating_point T> struct SvfOutputs {
    T low, band, high;
    T k; // the damping in use, for the derived outputs
    [[nodiscard]] T notch() const { return low + high; }
    [[nodiscard]] T peak() const { return low - high; }
    [[nodiscard]] T allpass() const { return low + high - k * band; }
    [[nodiscard]] T bandUnity() const { return k * band; } // band-pass with 0 dB at the center
};

template <std::floating_point T = float> class Svf {
public:
    // q > 0; q = 0.5 is critically damped, 0.7071 Butterworth.
    void setCutoff(double cutoffHz, double q, double sampleRate) {
        double r = cutoffHz / sampleRate;
        r = r < 1e-6 ? 1e-6 : (r > 0.49 ? 0.49 : r);
        setGk(static_cast<T>(math::tanD(math::kPi * r)), static_cast<T>(1.0 / q));
    }
    // g = tan(pi fc / fs) and k = 1 / Q directly (g > 0, k >= 0).
    void setGk(T g, T k) {
        g_ = g;
        k_ = k;
        a1_ = T(1) / (T(1) + g_ * (g_ + k_));
    }
    void reset() { ic1_ = ic2_ = T(0); }

    SvfOutputs<T> tick(T x) {
        const T v3 = x - ic2_;
        const T v1 = a1_ * (ic1_ + g_ * v3);
        const T v2 = ic2_ + g_ * v1;
        ic1_ = T(2) * v1 - ic1_;
        ic2_ = T(2) * v2 - ic2_;
        return {v2, v1, x - k_ * v1 - v2, k_};
    }

private:
    T g_ = T(0.1), k_ = T(2), a1_ = T(1) / (T(1) + T(0.1) * (T(0.1) + T(2)));
    T ic1_ = T(0), ic2_ = T(0);
};

} // namespace spux::inline v1::dsp
