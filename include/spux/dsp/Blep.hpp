// Band-limited step residuals (BLEP) of orders 2, 4, 16 and 32. The
// minimum-phase pt2-clone table is separate, under its own
// license, in spux/dsp/pt2/MinBlep.hpp.
//
// residual(x) is the band-limited step minus the ideal step, with x the signed distance in samples from
// the discontinuity (positive after it). Add jump * residual(x) to the naive waveform at every sample
// with |x| < kWidth. The residual is zero outside (-kWidth, kWidth).
//
//   Blep<2>   2-sample PolyBLEP (triangle kernel).
//   Blep<4>   4-sample PolyBLEP (cubic B-spline kernel).
//   Blep<16>  a 16-sample table kernel: Kaiser-windowed sinc (beta 7, cutoff 0.36 of the sampling rate)
//             of unit area, integrated into a step, 64 points per sample, linear interpolation.
//   Blep<32>  the same kernel stretched for 2x rates (cutoff 0.18), so a 32-sample kernel at twice the
//             host rate band-limits like the 16-sample one at the host rate.
//
// The tables are built once when the binary loads (dynamic initialization of an immutable object, never
// on the audio thread) with spux math.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>

#include "spux/core/Math.hpp"

namespace spux::inline v1::dsp {

template <int Order, std::floating_point T = float> struct Blep;

template <std::floating_point T> struct Blep<2, T> {
    static constexpr T kWidth = T(1);
    static constexpr T residual(T x) {
        if (x <= T(-1) || x >= T(1))
            return T(0);
        if (x < T(0)) {
            const T u = T(1) + x;
            return T(0.5) * u * u;
        }
        const T u = T(1) - x;
        return T(-0.5) * u * u;
    }
};

template <std::floating_point T> struct Blep<4, T> {
    static constexpr T kWidth = T(2);
    static constexpr T residual(T x) {
        if (x <= T(-2) || x >= T(2))
            return T(0);
        if (x < T(-1)) {
            const T u = T(2) + x;
            const T u2 = u * u;
            return u2 * u2 * (T(1) / T(24));
        }
        const T x2 = x * x, x3 = x2 * x, x4 = x2 * x2;
        if (x < T(0))
            return T(0.5) + x * (T(2) / T(3)) - x3 * (T(1) / T(3)) - x4 * T(0.125);
        if (x < T(1))
            return T(-0.5) + x * (T(2) / T(3)) - x3 * (T(1) / T(3)) + x4 * T(0.125);
        const T u = T(2) - x;
        const T u2 = u * u;
        return -u2 * u2 * (T(1) / T(24));
    }
};

// Step response S(x) of the windowed-sinc kernel over [-HalfWidth, HalfWidth], 64 points per sample.
template <int HalfWidth> struct BlepTable {
    static constexpr int kHalfWidth = HalfWidth;
    static constexpr int kOversample = 64;
    static constexpr int kPoints = 2 * kHalfWidth * kOversample + 1;
    std::array<float, kPoints> step{};

    BlepTable() {
        constexpr double beta = 7.0;
        const double fc = 0.36 * 8.0 / HalfWidth;
        const auto i0 = [](double x) {
            double s = 1, t = 1;
            for (int k = 1; k < 40; ++k) {
                t *= (x / (2 * k)) * (x / (2 * k));
                s += t;
            }
            return s;
        };
        std::array<double, kPoints> k{};
        double sum = 0;
        for (int i = 0; i < kPoints; ++i) {
            const double x = static_cast<double>(i - kPoints / 2) / kOversample;
            const double a = 2.0 * math::kPi * fc * x;
            const double sinc = x == 0.0 ? 1.0 : math::sinD(a) / a;
            const double r = x / kHalfWidth;
            // sqrt is IEEE-exact (an instruction), not a libm approximation
            k[static_cast<std::size_t>(i)] =
                2.0 * fc * sinc * i0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / i0(beta);
            sum += k[static_cast<std::size_t>(i)];
        }
        double acc = 0;
        for (int i = 0; i < kPoints; ++i) { // trapezoid integral, normalized to unit area
            acc += 0.5 * k[static_cast<std::size_t>(i)] / sum;
            step[static_cast<std::size_t>(i)] = static_cast<float>(acc);
            acc += 0.5 * k[static_cast<std::size_t>(i)] / sum;
        }
        step[0] = 0.0f;
        step[kPoints - 1] = 1.0f;
    }
};

template <int HalfWidth> inline const BlepTable<HalfWidth> kBlepTable;

template <int Order, std::floating_point T> struct BlepTableKernel {
    static constexpr int kHalf = Order / 2;
    static constexpr T kWidth = static_cast<T>(kHalf);
    // The step is interpolated and the ideal step subtracted afterwards, so the residual is continuous.
    static T residual(T x) {
        if (x <= -kWidth || x >= kWidth)
            return T(0);
        const auto& t = kBlepTable<kHalf>.step;
        const T p = (x + kWidth) * static_cast<T>(BlepTable<kHalf>::kOversample);
        const int i = static_cast<int>(p);
        const T f = p - static_cast<T>(i);
        const auto at = [&](int j) { return static_cast<T>(t[static_cast<std::size_t>(j)]); };
        const T s = at(i) + f * (at(i + 1) - at(i));
        return x >= T(0) ? s - T(1) : s;
    }
};

template <std::floating_point T> struct Blep<16, T> : BlepTableKernel<16, T> {};
template <std::floating_point T> struct Blep<32, T> : BlepTableKernel<32, T> {};

} // namespace spux::inline v1::dsp
