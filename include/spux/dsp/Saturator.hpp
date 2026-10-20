// Saturator policies, first-order antiderivative anti-aliasing (ADAA) and the implicit solvers of
// zero-delay feedback loops: the shape and
// the solver are template parameters of the block that uses them, stated by concepts.
//
// A Saturator provides value(x) and deriv(x) for float and double; an AdaaSaturator also provides
// antiderivative(x) in double, the integral of value from 0 (so antiderivative(0) = 0).
//
// Anti-aliasing: a static shape aliases. Run it through Adaa1 (first-order ADAA) inside an Oversampler
// (Halfband.hpp). tests/saturator_test.cpp measures a 3.5 kHz sine driven to 6 at 48 kHz: ADAA alone
// lowers the total alias power by about 5.5 dB, 4x oversampling by 32 to 44 dB, and ADAA on top of 4x by
// 0.2 dB (smooth shapes) to 12 dB (hard clip). Adaa1 delays the signal by half a sample.
//
//   TanhSaturator           tanh (fastTanhF, abs. error < 1e-6); antiderivative log cosh.
//   AsymmetricTanhSaturator tanh for x >= 0, 1.3 tanh(0.7 x) below (a diode-clipper asymmetry).
//   HardClipSaturator       clamp to [-1, 1].
//   PadeSaturator           x (27 + x^2) / (27 + 9 x^2), +-1 from |x| = 3.
// The shapes use spux math; they differ from std::tanh by about 1e-7.
#pragma once

#include <concepts>

#include "spux/core/Math.hpp"

namespace spux::inline v1::dsp {

template <class S, class T>
concept Saturator = std::floating_point<T> && requires(T x) {
    { S::value(x) } -> std::same_as<T>;
    { S::deriv(x) } -> std::same_as<T>;
};

template <class S, class T>
concept AdaaSaturator = Saturator<S, T> && requires(double x) {
    { S::antiderivative(x) } -> std::same_as<double>;
};

namespace detail {
template <std::floating_point T> constexpr T absT(T x) { return x < T(0) ? -x : x; }
// tanh in T: fastTanhF for float; for double through expD (error below 1e-15).
template <std::floating_point T> constexpr T tanhT(T x) {
    if constexpr (std::same_as<T, float>) {
        return math::fastTanhF(x);
    } else {
        if (x > T(20))
            return T(1);
        if (x < T(-20))
            return T(-1);
        const T e = static_cast<T>(math::expD(2.0 * static_cast<double>(x)));
        return (e - T(1)) / (e + T(1));
    }
}
// log(1 + u) for u in [0, 3], from log1pUnit (absolute error < 1e-8).
constexpr double log1pToThree(double u) {
    return u <= 1.0 ? math::log1pUnit(u) : math::kLn2 + math::log1pUnit((u - 1.0) * 0.5);
}
} // namespace detail

struct TanhSaturator {
    template <std::floating_point T> static constexpr T value(T x) { return detail::tanhT(x); }
    template <std::floating_point T> static constexpr T deriv(T x) {
        const T t = detail::tanhT(x);
        return T(1) - t * t;
    }
    static constexpr double antiderivative(double x) { return math::logCosh(x); }
};

struct AsymmetricTanhSaturator {
    static constexpr double kNegSlope = 0.7;
    static constexpr double kNegScale = 1.3;

    template <std::floating_point T> static constexpr T value(T x) {
        if (x >= T(0))
            return detail::tanhT(x);
        return T(kNegScale) * detail::tanhT(T(kNegSlope) * x);
    }
    template <std::floating_point T> static constexpr T deriv(T x) {
        if (x >= T(0)) {
            const T t = detail::tanhT(x);
            return T(1) - t * t;
        }
        const T t = detail::tanhT(T(kNegSlope) * x);
        return T(kNegScale) * T(kNegSlope) * (T(1) - t * t);
    }
    static constexpr double antiderivative(double x) {
        if (x >= 0.0)
            return math::logCosh(x);
        return kNegScale / kNegSlope * math::logCosh(kNegSlope * x);
    }
};

struct HardClipSaturator {
    template <std::floating_point T> static constexpr T value(T x) { return x > T(1) ? T(1) : (x < T(-1) ? T(-1) : x); }
    template <std::floating_point T> static constexpr T deriv(T x) { return detail::absT(x) < T(1) ? T(1) : T(0); }
    static constexpr double antiderivative(double x) {
        const double a = detail::absT(x);
        return a <= 1.0 ? 0.5 * x * x : a - 0.5;
    }
};

struct PadeSaturator {
    template <std::floating_point T> static constexpr T value(T x) { return math::tanhPade32(x); }
    // 9 (9 - x^2)^2 / (27 + 9 x^2)^2 inside |x| < 3, 0 outside.
    template <std::floating_point T> static constexpr T deriv(T x) {
        if (detail::absT(x) >= T(3))
            return T(0);
        const T x2 = x * x;
        const T d = T(27) + T(9) * x2;
        const T n = T(9) - x2;
        return T(9) * n * n / (d * d);
    }
    // x^2 / 18 + (4 / 3) log(1 + x^2 / 3) inside |x| < 3; linear beyond.
    static constexpr double antiderivative(double x) {
        const double a = detail::absT(x);
        if (a < 3.0)
            return a * a / 18.0 + (4.0 / 3.0) * detail::log1pToThree(a * a / 3.0);
        return a - 3.0 + (0.5 + (4.0 / 3.0) * detail::log1pToThree(3.0));
    }
};

// First-order ADAA: y = (F(x) - F(x1)) / (x - x1), and value((x + x1) / 2) when |x - x1| < Epsilon (the
// difference quotient loses accuracy there). The state is double.
template <class S, std::floating_point T = float, double Epsilon = 5e-3>
    requires AdaaSaturator<S, T> && (Epsilon > 0.0)
class Adaa1 {
public:
    void reset(T x0 = T(0)) {
        x1_ = static_cast<double>(x0);
        f1_ = S::antiderivative(x1_);
    }

    T process(T xIn) {
        const auto x = static_cast<double>(xIn);
        const double f = S::antiderivative(x);
        const double dx = x - x1_;
        double y;
        if (detail::absT(dx) < Epsilon)
            y = S::template value<double>(0.5 * (x + x1_));
        else
            y = (f - f1_) / dx;
        x1_ = x;
        f1_ = f;
        return static_cast<T>(y);
    }

private:
    double x1_ = 0.0, f1_ = 0.0;
};

// Solvers of the implicit feedback equation y + G S(k y) = f0.
template <class P, class T>
concept ImplicitSolver = std::floating_point<T> && requires(T f0, T G, T k, T guess) {
    { P::solve(f0, G, k, guess) } -> std::same_as<T>;
};

// Cheap: no derivative, no convergence check (converges for |G k S'| < 1).
template <class S, int Iterations>
    requires(Iterations > 0)
struct FixedPointSolver {
    template <std::floating_point T>
        requires Saturator<S, T>
    static constexpr T solve(T f0, T G, T k, T guess) {
        T y = guess;
        for (int i = 0; i < Iterations; ++i)
            y = f0 - G * S::value(k * y);
        return y;
    }
};

// Newton's method, at most Iterations steps, stops once a step is below 1e-6.
template <class S, int Iterations>
    requires(Iterations > 0)
struct NewtonSolver {
    template <std::floating_point T>
        requires Saturator<S, T>
    static constexpr T solve(T f0, T G, T k, T guess) {
        T y = guess;
        for (int i = 0; i < Iterations; ++i) {
            const T sat = S::value(k * y);
            const T fVal = y + G * sat - f0;
            const T fDev = T(1) + G * k * S::deriv(k * y);
            const T step = fVal / fDev;
            y -= step;
            if (detail::absT(step) < T(1.0e-6f))
                break;
        }
        return y;
    }
};

} // namespace spux::inline v1::dsp
