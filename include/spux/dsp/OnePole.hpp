// Zero-delay-feedback one-pole filter, trapezoidal (TPT, Zavalishin): low-pass, high-pass and all-pass
// from one state. Stable under coefficient changes every sample.
//
// Law: g = tan(pi fc / fs), G = g / (1 + g); v = (x - s) G, lp = v + s, s' = lp + v; hp = x - lp,
// ap = lp - hp. The cutoff is clamped to [1e-6, 0.49] of the sample rate. Same arithmetic as Lumeoid
// `fx::OnePole`, Tannhauser `OnePoleLp` and Plumeflow's tone filters.
#pragma once

#include <concepts>

#include "spux/core/Math.hpp"

namespace spux::inline v1::dsp {

// G = g / (1 + g) for a cutoff in Hz, computed in double.
[[nodiscard]] constexpr double onePoleG(double cutoffHz, double sampleRate) {
    double r = cutoffHz / sampleRate;
    r = r < 1e-6 ? 1e-6 : (r > 0.49 ? 0.49 : r);
    const double g = math::tanD(math::kPi * r);
    return g / (1.0 + g);
}

template <std::floating_point T = float> class OnePoleTpt {
public:
    void setCutoff(double cutoffHz, double sampleRate) { G_ = static_cast<T>(onePoleG(cutoffHz, sampleRate)); }
    // G = g / (1 + g) directly, for code that computes it itself (0 < G < 1).
    void setG(T G) { G_ = G; }
    [[nodiscard]] T G() const { return G_; }
    void reset(T state = T(0)) { s_ = state; }
    [[nodiscard]] T state() const { return s_; }

    T lowpass(T x) {
        const T v = (x - s_) * G_;
        const T lp = v + s_;
        s_ = lp + v;
        return lp;
    }
    T highpass(T x) { return x - lowpass(x); }
    T allpass(T x) {
        const T lp = lowpass(x);
        return lp - (x - lp);
    }

private:
    T G_ = T(0.5);
    T s_ = T(0);
};

} // namespace spux::inline v1::dsp
