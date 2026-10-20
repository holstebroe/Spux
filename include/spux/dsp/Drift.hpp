// Ornstein-Uhlenbeck process for slow random drift (oscillator pitch, analog wobble).
// Exact discretization: x' = a x + sigma sqrt(1 - a^2) n with a = e^(-dt / tau)
// and n Gaussian (Pcg32::nextGaussian), so the stationary RMS is sigma for any step dt. Step it at the
// control tick (dt = tick samples / sample rate) or per sample; the law holds for any dt (R4).
// What sigma means (cents, volts) and how a knob maps to it stays in the plugin. setup() uses expD.
#pragma once

#include <cmath>
#include <concepts>

#include "spux/core/Math.hpp"
#include "spux/core/Pcg32.hpp"

namespace spux::inline v1::dsp {

template <std::floating_point T = double> class OuProcess {
public:
    // sigma: stationary RMS; tau and dt in seconds (tau > 0). Control rate, never per sample.
    void setup(double sigma, double tauSeconds, double dtSeconds) {
        sigma_ = sigma;
        const double a = math::expD(-dtSeconds / tauSeconds);
        a_ = static_cast<T>(a);
        b_ = static_cast<T>(sigma * std::sqrt(1.0 - a * a)); // sqrt is IEEE-exact, not libm
    }
    // Starts in the stationary distribution.
    void reset(Pcg32& rng) { x_ = static_cast<T>(sigma_ * rng.nextGaussian()); }
    void set(T value) { x_ = value; }
    T step(Pcg32& rng) {
        x_ = a_ * x_ + b_ * static_cast<T>(rng.nextGaussian());
        return x_;
    }
    [[nodiscard]] T value() const { return x_; }

private:
    double sigma_ = 1.0;
    T a_ = T(0), b_ = T(1), x_ = T(0);
};

// An OU process with its own generator: seed it per voice and note, then step() at the control tick.
template <std::floating_point T = double> class OuDrift {
public:
    void seed(const Pcg32& rng) { rng_ = rng; }
    void setup(double sigma, double tauSeconds, double dtSeconds) { ou_.setup(sigma, tauSeconds, dtSeconds); }
    void reset() { ou_.reset(rng_); }
    T step() { return ou_.step(rng_); }
    [[nodiscard]] T value() const { return ou_.value(); }

private:
    OuProcess<T> ou_;
    Pcg32 rng_;
};

} // namespace spux::inline v1::dsp
