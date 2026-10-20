// Biquad in transposed direct form II with the RBJ cookbook designs (Bristow-Johnson, "Audio EQ
// Cookbook"). Designs are computed in double with spux math and stored as T; Tannhauser's `Biquad` is
// Biquad<double> with designLowpass.
//
// Law (all designs): w = 2 pi fc / fs with fc / fs clamped to [1e-5, 0.49], alpha = sin w / (2 Q),
// A = 10^(dB / 40) for peaking and shelves (shelf slope S = 1); coefficients normalized by a0.
//   y = b0 x + z1; z1' = b1 x - a1 y + z2; z2' = b2 x - a2 y.
#pragma once

#include <cmath>
#include <concepts>

#include "spux/core/Math.hpp"

namespace spux::inline v1::dsp {

struct BiquadCoeffs {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

namespace detail {
struct RbjAngles {
    double cw, sw;
};
inline RbjAngles rbjAngles(double cutoffHz, double sampleRate) {
    double r = cutoffHz / sampleRate;
    r = r < 1e-5 ? 1e-5 : (r > 0.49 ? 0.49 : r);
    const double w = math::kTwoPi * r;
    return {math::cosD(w), math::sinD(w)};
}
inline BiquadCoeffs normalize(double b0, double b1, double b2, double a0, double a1, double a2) {
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}
inline double dbAmplitude40(double gainDb) { return math::expD(gainDb / 40.0 * math::kLn10); } // 10^(dB/40)
} // namespace detail

inline BiquadCoeffs designLowpass(double fc, double q, double fs) {
    const auto [cw, sw] = detail::rbjAngles(fc, fs);
    const double alpha = sw / (2.0 * q);
    return detail::normalize((1.0 - cw) * 0.5, 1.0 - cw, (1.0 - cw) * 0.5, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
}
inline BiquadCoeffs designHighpass(double fc, double q, double fs) {
    const auto [cw, sw] = detail::rbjAngles(fc, fs);
    const double alpha = sw / (2.0 * q);
    return detail::normalize((1.0 + cw) * 0.5, -(1.0 + cw), (1.0 + cw) * 0.5, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
}
// Band-pass with 0 dB at the centre (constant peak gain).
inline BiquadCoeffs designBandpass(double fc, double q, double fs) {
    const auto [cw, sw] = detail::rbjAngles(fc, fs);
    const double alpha = sw / (2.0 * q);
    return detail::normalize(alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
}
inline BiquadCoeffs designNotch(double fc, double q, double fs) {
    const auto [cw, sw] = detail::rbjAngles(fc, fs);
    const double alpha = sw / (2.0 * q);
    return detail::normalize(1.0, -2.0 * cw, 1.0, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
}
inline BiquadCoeffs designAllpass(double fc, double q, double fs) {
    const auto [cw, sw] = detail::rbjAngles(fc, fs);
    const double alpha = sw / (2.0 * q);
    return detail::normalize(1.0 - alpha, -2.0 * cw, 1.0 + alpha, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
}
inline BiquadCoeffs designPeak(double fc, double q, double gainDb, double fs) {
    const auto [cw, sw] = detail::rbjAngles(fc, fs);
    const double alpha = sw / (2.0 * q);
    const double A = detail::dbAmplitude40(gainDb);
    return detail::normalize(1.0 + alpha * A, -2.0 * cw, 1.0 - alpha * A, 1.0 + alpha / A, -2.0 * cw, 1.0 - alpha / A);
}
inline BiquadCoeffs designLowShelf(double fc, double gainDb, double fs) {
    const auto [cw, sw] = detail::rbjAngles(fc, fs);
    const double A = detail::dbAmplitude40(gainDb);
    const double alpha = sw * 0.5 * std::sqrt(2.0); // S = 1
    const double sa = 2.0 * std::sqrt(A) * alpha;   // sqrt is IEEE-exact, not a libm approximation
    return detail::normalize(A * ((A + 1.0) - (A - 1.0) * cw + sa), 2.0 * A * ((A - 1.0) - (A + 1.0) * cw),
                             A * ((A + 1.0) - (A - 1.0) * cw - sa), (A + 1.0) + (A - 1.0) * cw + sa,
                             -2.0 * ((A - 1.0) + (A + 1.0) * cw), (A + 1.0) + (A - 1.0) * cw - sa);
}
inline BiquadCoeffs designHighShelf(double fc, double gainDb, double fs) {
    const auto [cw, sw] = detail::rbjAngles(fc, fs);
    const double A = detail::dbAmplitude40(gainDb);
    const double alpha = sw * 0.5 * std::sqrt(2.0);
    const double sa = 2.0 * std::sqrt(A) * alpha;
    return detail::normalize(A * ((A + 1.0) + (A - 1.0) * cw + sa), -2.0 * A * ((A - 1.0) + (A + 1.0) * cw),
                             A * ((A + 1.0) + (A - 1.0) * cw - sa), (A + 1.0) - (A - 1.0) * cw + sa,
                             2.0 * ((A - 1.0) - (A + 1.0) * cw), (A + 1.0) - (A - 1.0) * cw - sa);
}

template <std::floating_point T = float> class Biquad {
public:
    void set(const BiquadCoeffs& c) {
        b0_ = static_cast<T>(c.b0);
        b1_ = static_cast<T>(c.b1);
        b2_ = static_cast<T>(c.b2);
        a1_ = static_cast<T>(c.a1);
        a2_ = static_cast<T>(c.a2);
    }
    void reset() { z1_ = z2_ = T(0); }

    T tick(T x) {
        const T y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        return y;
    }
    void process(T* data, int n) {
        for (int i = 0; i < n; ++i)
            data[i] = tick(data[i]);
    }

private:
    T b0_ = T(1), b1_ = T(0), b2_ = T(0), a1_ = T(0), a2_ = T(0);
    T z1_ = T(0), z2_ = T(0);
};

} // namespace spux::inline v1::dsp
