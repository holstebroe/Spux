// Deterministic math (R2, R3, decision S4): fixed formulas with no libm call, so a result depends only on
// the build, never on the platform's math library. Opt-in: libm is the default in the audio path (its
// differences between platforms are inaudible); use these where spux_bench shows a gain that matters, where
// constexpr is needed, or where the same bits on every platform are wanted. Merged from Lumeoid
// `dsp/math.hpp` (the double-precision `...D` set, the [7/6] Pade tanh) and Plumeflow `dsp/Math.hpp` (the
// float set fitted on Chebyshev nodes by its tools/fit_math.py), plus Tannhauser's [3/2] Pade tanh.
//
// Two precision tiers:
//   - double (`expD`, `exp2D`, `logD`, `log2D` to about 1e-14 relative; `sinD`, `cosD` to 6e-10 absolute;
//     `tanD` to 3e-8 relative near pi/2): coefficients, tables, phase. Series with a fixed number of terms; slower than
//     libm (see docs/BENCH.md), so use them at control rate.
//   - float, to about 1e-6 (`exp2F`, `fastTanhF`, `sinTurns`, `tanRad`, ...): per sample. Less precise than
//     libm (one to two ulp, against libm's half to one); `exp2F` is also slower than glibc's.
// Every function is a fixed sequence of IEEE operations, constexpr where C++20 allows it. The measured
// errors are checked by tests/math_test.cpp and the speed against libm by spux_bench.
//
// Bit compatibility: each function performs the same operations in the same order as the original it
// came from, so a plugin that switches gets identical results (checked against the originals when they
// were merged). The only differences: floor is done by integer conversion instead of std::floor (same
// result), NaN input to `exp2F` gives 2^-126 instead of undefined behaviour, and the renamed functions
// (Plumeflow `exp2`, `tanh` are `exp2F`, `fastTanhF`; Lumeoid `tanhApprox` is `tanhPade76`; Tannhauser
// `tanhFast` is `tanhPade32`), so `using namespace` never collides with <cmath>.
#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstdint>

namespace spux::inline v1::math {

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kTwoPi = 2.0 * kPi;
inline constexpr double kLn2 = 0.69314718055994530942;
inline constexpr double kLn10 = 2.30258509299404568402;
inline constexpr double kLog2E = 1.44269504088896340736;
inline constexpr float kPiF = 3.14159265358979323846f;

// floor(x) for any finite double, exact (doubles at or above 2^52 are integers already).
[[nodiscard]] constexpr double floorD(double x) {
    if (!(x > -4503599627370496.0 && x < 4503599627370496.0))
        return x;
    const auto i = static_cast<double>(static_cast<std::int64_t>(x));
    return i - static_cast<double>(i > x);
}

// floor(x) for |x| < 2^31, exact.
[[nodiscard]] constexpr float floorF(float x) {
    const auto i = static_cast<float>(static_cast<std::int32_t>(x));
    return i - static_cast<float>(i > x); // branchless: a random sign would mispredict half the time
}

// --- Double precision (Lumeoid) ------------------------------------------------------------------

// e^x. Range reduction to |r| <= ln2/2, then a degree-13 Taylor polynomial (relative error below 1e-15).
// Saturates at e^700; returns 0 below -700.
[[nodiscard]] constexpr double expD(double x) {
    if (x > 700.0)
        x = 700.0;
    if (x < -700.0)
        return 0.0;
    const double n = static_cast<double>(static_cast<std::int64_t>(x * kLog2E + (x >= 0 ? 0.5 : -0.5)));
    const double r = x - n * kLn2;
    double term = 1.0;
    double sum = 1.0;
    for (int k = 1; k <= 13; ++k) {
        term *= r / k;
        sum += term;
    }
    // Scale by 2^n through repeated exact doubling; n is bounded by |x| / ln2 <= 1011.
    auto scale = static_cast<std::int64_t>(n);
    double factor = 1.0;
    double base = scale >= 0 ? 2.0 : 0.5;
    if (scale < 0)
        scale = -scale;
    while (scale > 0) {
        if (scale & 1)
            factor *= base;
        base *= base;
        scale >>= 1;
    }
    return sum * factor;
}

[[nodiscard]] constexpr double exp2D(double x) { return expD(x * kLn2); }

// Natural logarithm for x > 0: split off powers of two, then 2 atanh((x - 1) / (x + 1)) as a series.
[[nodiscard]] constexpr double logD(double x) {
    int exponent = 0;
    while (x >= 2.0) {
        x *= 0.5;
        ++exponent;
    }
    while (x < 1.0) {
        x *= 2.0;
        --exponent;
    }
    const double s = (x - 1.0) / (x + 1.0);
    const double s2 = s * s;
    double term = s;
    double sum = 0.0;
    for (int k = 1; k <= 61; k += 2) {
        sum += term / k;
        term *= s2;
    }
    return 2.0 * sum + exponent * kLn2;
}

[[nodiscard]] constexpr double log2D(double x) { return logD(x) * kLog2E; }

// sin(x) for any finite x: reduce to [-pi, pi], then a degree-19 Taylor series (absolute error below 6e-10,
// at the ends of the interval; Lumeoid's header said exact to double precision, its test checked 1e-9).
[[nodiscard]] constexpr double sinD(double x) {
    const double turns = x / kTwoPi;
    const double shifted = turns - static_cast<double>(static_cast<std::int64_t>(turns + (turns >= 0 ? 0.5 : -0.5)));
    const double r = shifted * kTwoPi; // [-pi, pi]
    const double r2 = r * r;
    double term = r;
    double sum = r;
    for (int k = 1; k <= 9; ++k) {
        term *= -r2 / static_cast<double>((2 * k) * (2 * k + 1));
        sum += term;
    }
    return sum;
}

[[nodiscard]] constexpr double cosD(double x) { return sinD(x + kPi * 0.5); }

// tan(x) for |x| < pi/2 (relative error below 3e-8 up to 1.55).
[[nodiscard]] constexpr double tanD(double x) { return sinD(x) / cosD(x); }

// --- Float, per sample (Plumeflow) ------------------------------------------------------------

// 2^x, relative error < 2e-7 for |x| < 126; clamped to [-126, 126] (NaN gives 2^-126).
[[nodiscard]] constexpr float exp2F(float x) {
    x = x > -126.0f ? (x < 126.0f ? x : 126.0f) : -126.0f;
    const float fl = floorF(x);
    const float f = x - fl;
    float p = 0.001895107291004411f;
    p = p * f + 0.008946214666325987f;
    p = p * f + 0.05586328265926609f;
    p = p * f + 0.24014077009183496f;
    p = p * f + 0.6931546200033004f;
    p = p * f + 0.9999998957631361f;
    return p * std::bit_cast<float>(static_cast<std::uint32_t>(static_cast<std::int32_t>(fl) + 127) << 23);
}

// sin(2 pi t) with t in cycles (any magnitude); absolute error < 1e-6.
[[nodiscard]] constexpr float sinTurns(double t) {
    double r = t - floorD(t + 0.5); // [-0.5, 0.5]
    if (r > 0.25)
        r = 0.5 - r;
    else if (r < -0.25)
        r = -0.5 - r;
    const float x = static_cast<float>(r), x2 = x * x;
    float p = 39.53581371227564f;
    p = p * x2 - 76.54965682072039f;
    p = p * x2 + 81.60099819261687f;
    p = p * x2 - 41.341654929343555f;
    p = p * x2 + 6.283185159611169f;
    return p * x;
}
[[nodiscard]] constexpr float cosTurns(double t) { return sinTurns(t + 0.25); }

// tan(x) for x in (0, pi/2), relative error < 2e-6 up to 1.5 rad (filter coefficient g = tan(pi fc/fs)).
[[nodiscard]] constexpr float tanRad(float x) {
    const bool inv = x > 0.78539816f;
    const float y = inv ? 1.57079633f - x : x;
    const float y2 = y * y;
    float p = 0.020178050676800778f;
    p = p * y2 + 0.012882557377937889f;
    p = p * y2 + 0.05697183981504882f;
    p = p * y2 + 0.13290358894127094f;
    p = p * y2 + 0.3333541156875104f;
    p = p * y2 + 0.9999998984043268f;
    const float t = p * y;
    return inv ? 1.0f / t : t;
}

// tanh through exp2F, for hot loops: 1.6x (glibc) to 1.8x (MSVC) faster than std::tanh (docs/BENCH.md); in
// Acidus's ladder, about 150 calls per sample, that is 18 % of the engine. Absolute error below 1.2e-7 (libm:
// about 1e-7), exactly +-1 beyond |x| = 9; not odd, and fastTanhF(0) = -6e-8 (exp2F(0) is 1 - 1e-7).
[[nodiscard]] constexpr float fastTanhF(float x) {
    if (x > 9.0f)
        return 1.0f;
    if (x < -9.0f)
        return -1.0f;
    const float e = exp2F(x * 2.88539008f); // e^(2x)
    return (e - 1.0f) / (e + 1.0f);
}

// log2(x) for normal x > 0, absolute error < 3e-5 (mip selection, meters; not for coefficients).
[[nodiscard]] constexpr float log2Approx(float x) {
    const std::uint32_t b = std::bit_cast<std::uint32_t>(x);
    const float e = static_cast<float>(static_cast<std::int32_t>((b >> 23) & 255u) - 127);
    const float m = std::bit_cast<float>((b & 0x007FFFFFu) | 0x3F800000u); // [1, 2)
    float p = 0.04392862784798668f;
    p = p * m - 0.409475585766699f;
    p = p * m + 1.6101775468974293f;
    p = p * m - 3.5202188381463437f;
    p = p * m + 5.069756316633784f;
    p = p * m - 2.7941536765361388f;
    return e + p;
}

// atan(x), absolute error < 2e-6.
[[nodiscard]] constexpr float atanApprox(float x) {
    const float ax = x < 0.0f ? -x : x;
    const bool inv = ax > 1.0f;
    const float t = inv ? 1.0f / ax : ax, t2 = t * t;
    float p = -0.011770499896159947f;
    p = p * t2 + 0.0528234878499141f;
    p = p * t2 - 0.1166511163232199f;
    p = p * t2 + 0.19367031614042268f;
    p = p * t2 - 0.33265548273235135f;
    p = p * t2 + 0.9999798340122587f;
    float r = p * t;
    if (inv)
        r = 1.57079633f - r;
    return x < 0.0f ? -r : r;
}

// tanh(x) / x: the gain of a saturating stage at amplitude x (series near zero); absolute error below
// 3e-5 (fastTanhF's error divided by a small x).
[[nodiscard]] constexpr float tanhGain(float x) {
    const float a = x < 0.0f ? -x : x;
    if (a < 1e-3f)
        return 1.0f - x * x * (1.0f / 3.0f);
    return fastTanhF(x) / x;
}

namespace detail {
inline constexpr std::array<double, 9> kLog1pUnit = {0.9999992249459301,   -0.49996777732607206, 0.3328626878690843,
                                                     -0.24654841019911702, 0.18517671304827893,  -0.12601773503348934,
                                                     0.06719921822908655,  -0.02338164939671636, 0.003824912523750897};
}

// log(1 + u) for u in [0, 1], absolute error < 6e-9.
[[nodiscard]] constexpr double log1pUnit(double u) {
    double p = detail::kLog1pUnit[8];
    for (int i = 7; i >= 0; --i)
        p = p * u + detail::kLog1pUnit[static_cast<std::size_t>(i)];
    return p * u;
}

// log(cosh(x)), the antiderivative of tanh, for any |x| (no overflow): |x| + log(1 + e^-2|x|) - ln 2.
// Absolute error below 1e-7 (exp2F inside); used by first-order ADAA of tanh.
[[nodiscard]] constexpr double logCosh(double x) {
    const double a = x < 0.0 ? -x : x;
    const double u =
        static_cast<double>(exp2F(static_cast<float>(-2.88539008177792681 * (a > 30.0 ? 30.0 : a)))); // e^(-2a)
    return a + log1pUnit(u) - 0.6931471805599453;
}

// Soft limiter for modulation sums: linear below 0.75, then a smooth knee that approaches +-1.
[[nodiscard]] constexpr float softLimit(float x) {
    const float a = x < 0.0f ? -x : x;
    if (a <= 0.75f)
        return x;
    const float y = 0.75f + 0.25f * fastTanhF((a - 0.75f) * 4.0f);
    return x < 0.0f ? -y : y;
}

// 10^(dB / 20) through exp2F (relative error < 1e-6 for -120..40 dB).
[[nodiscard]] constexpr float dbToGainF(float db) { return exp2F(db * 0.16609640474f); }

// --- Rational tanh shapes ------------------------------------------------------------------------

// tanh(x) by the [7/6] Pade approximant on |x| < 4.97, saturating beyond; absolute error below 1e-4
// over the whole real line (Lumeoid `tanhApprox`). For T = float it is bit-identical to the original.
template <std::floating_point T> [[nodiscard]] constexpr T tanhPade76(T x) {
    constexpr T kLimit = T(4.97f);
    constexpr T kSaturation = T(1.0f - 9.6e-5f);
    if (x >= kLimit)
        return kSaturation;
    if (x <= -kLimit)
        return -kSaturation;
    const T x2 = x * x;
    const T num = x * (T(135135) + x2 * (T(17325) + x2 * (T(378) + x2)));
    const T den = T(135135) + x2 * (T(62370) + x2 * (T(3150) + x2 * T(28)));
    return num / den;
}

// The [3/2] Pade soft clipper x (27 + x^2) / (27 + 9 x^2), exactly +-1 from |x| = 3 (Tannhauser `tanhFast`).
// Not an approximation of tanh to any tolerance: a shape of its own (error up to 0.02 near |x| = 1.5).
template <std::floating_point T> [[nodiscard]] constexpr T tanhPade32(T x) {
    if (x >= T(3))
        return T(1);
    if (x <= T(-3))
        return T(-1);
    const T x2 = x * x;
    return x * (T(27) + x2) / (T(27) + T(9) * x2);
}

} // namespace spux::inline v1::math
