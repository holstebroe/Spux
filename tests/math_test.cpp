// Error bounds of the deterministic math set (spux/core/Math.hpp) against libm. The test may call libm;
// the audio path may not (R2).
#include <doctest.h>

#include <cmath>
#include <limits>

#include "spux/core/Math.hpp"

using namespace spux::math;

namespace {
template <class F> double worstRelative(double lo, double hi, double step, F&& f) {
    double worst = 0.0;
    for (double x = lo; x <= hi; x += step)
        worst = std::max(worst, f(x));
    return worst;
}
} // namespace

TEST_CASE("double set: exp, exp2, log, log2 to about 1e-14; sin, cos 6e-10; tan 3e-8 relative") {
    CHECK(worstRelative(-700.0, 700.0, 0.0731, [](double x) { return std::fabs(expD(x) / std::exp(x) - 1.0); }) <
          1e-13);
    CHECK(worstRelative(-60.0, 60.0, 0.0137, [](double x) { return std::fabs(exp2D(x) / std::exp2(x) - 1.0); }) <
          1e-13);
    double worst = 0.0;
    for (double x = 1e-300; x < 1e300; x *= 1.37)
        worst = std::max(worst, std::fabs(logD(x) - std::log(x)) / std::max(1.0, std::fabs(std::log(x))));
    CHECK(worst < 1e-14);
    worst = 0.0;
    for (double x = 1e-6; x < 1e6; x *= 1.0371)
        worst = std::max(worst, std::fabs(log2D(x) - std::log2(x)));
    CHECK(worst < 1e-12);
    CHECK(worstRelative(-1000.0, 1000.0, 0.0071, [](double x) { return std::fabs(sinD(x) - std::sin(x)); }) < 6e-10);
    CHECK(worstRelative(-50.0, 50.0, 0.0071, [](double x) { return std::fabs(cosD(x) - std::cos(x)); }) < 6e-10);
    CHECK(worstRelative(-1.55, 1.55, 0.0031, [](double x) { return std::fabs(tanD(x) / std::tan(x) - 1.0); }) < 3e-8);
    CHECK(expD(-701.0) == 0.0);
    CHECK(expD(0.0) == 1.0);
}

TEST_CASE("float set: error bounds") {
    double worst = 0.0;
    for (float x = -125.0f; x <= 125.0f; x += 0.00731f)
        worst = std::max(worst, std::fabs(exp2F(x) / std::exp2(static_cast<double>(x)) - 1.0));
    CHECK(worst < 2e-7);
    CHECK(exp2F(std::numeric_limits<float>::quiet_NaN()) == exp2F(-126.0f)); // NaN is clamped, never UB

    worst = 0.0;
    for (double t = -1000.0; t <= 1000.0; t += 0.000731)
        worst = std::max(worst, std::fabs(static_cast<double>(sinTurns(t)) - std::sin(2.0 * kPi * t)));
    CHECK(worst < 1e-6);
    CHECK(cosTurns(0.0) == doctest::Approx(1.0f).epsilon(1e-6));

    worst = 0.0;
    for (float x = 1e-3f; x <= 1.5f; x += 0.0007f)
        worst = std::max(worst, std::fabs(tanRad(x) / std::tan(static_cast<double>(x)) - 1.0));
    CHECK(worst < 2e-6);

    worst = 0.0;
    for (float x = -12.0f; x <= 12.0f; x += 0.0037f)
        worst = std::max(worst, std::fabs(static_cast<double>(tanhF(x)) - std::tanh(static_cast<double>(x))));
    CHECK(worst < 1e-6);
    CHECK(tanhF(10.0f) == 1.0f);

    worst = 0.0;
    for (float x = 1e-6f; x < 1e6f; x *= 1.003f)
        worst = std::max(worst, std::fabs(static_cast<double>(log2Approx(x)) - std::log2(static_cast<double>(x))));
    CHECK(worst < 3e-5);

    worst = 0.0;
    for (float x = -50.0f; x <= 50.0f; x += 0.0013f)
        worst = std::max(worst, std::fabs(static_cast<double>(atanApprox(x)) - std::atan(static_cast<double>(x))));
    CHECK(worst < 2e-6);

    worst = 0.0;
    for (float x = -8.0f; x <= 8.0f; x += 0.0011f) {
        const double ref = x == 0.0f ? 1.0 : std::tanh(static_cast<double>(x)) / x;
        worst = std::max(worst, std::fabs(static_cast<double>(tanhGain(x)) - ref));
    }
    CHECK(worst < 3e-5); // tanhF's absolute error divided by a small x

    worst = 0.0;
    for (double u = 0.0; u <= 1.0; u += 1e-4)
        worst = std::max(worst, std::fabs(log1pUnit(u) - std::log1p(u)));
    CHECK(worst < 6e-9);

    worst = 0.0;
    for (double x = -40.0; x <= 40.0; x += 0.0013) {
        const double a = std::fabs(x);
        const double ref = a + std::log1p(std::exp(-2.0 * a)) - std::log(2.0);
        worst = std::max(worst, std::fabs(logCosh(x) - ref));
    }
    CHECK(worst < 1e-7);

    worst = 0.0;
    for (float db = -120.0f; db <= 40.0f; db += 0.013f)
        worst = std::max(worst, std::fabs(dbToGainF(db) / std::pow(10.0, db / 20.0) - 1.0));
    CHECK(worst < 1e-6);
}

TEST_CASE("rational shapes") {
    double worst = 0.0;
    for (double x = -8.0; x <= 8.0; x += 1e-3)
        worst = std::max(worst, std::fabs(static_cast<double>(tanhPade76(static_cast<float>(x))) - std::tanh(x)));
    CHECK(worst < 1e-4);
    CHECK(tanhPade76(10.0) == static_cast<double>(1.0f - 9.6e-5f)); // the float constant, for any T

    CHECK(tanhPade32(3.0) == 1.0);
    CHECK(tanhPade32(-7.0f) == -1.0f);
    CHECK(tanhPade32(1.0) == doctest::Approx(28.0 / 36.0));
    // monotonic (up to float rounding at the flat knee) and continuous at the knee
    double prev = -1.0;
    for (double x = -4.0; x <= 4.0; x += 1e-3) {
        const double y = tanhPade32(x);
        CHECK(y >= prev - 1e-12);
        prev = y;
    }
    CHECK(std::fabs(tanhPade32(2.9999f) - 1.0f) < 1e-6f);
}

TEST_CASE("floor helpers are exact") {
    for (double x : {-3.5, -3.0, -0.0, 0.0, 0.25, 2.0, 1e15 + 0.5, -1e15 - 0.5, 1e300, -1e300})
        CHECK(floorD(x) == std::floor(x));
    for (float x : {-3.5f, -3.0f, 0.0f, 0.75f, 125.9f, -125.9f})
        CHECK(floorF(x) == std::floor(x));
}

TEST_CASE("constexpr") {
    static_assert(expD(0.0) == 1.0);
    static_assert(exp2F(1.0f) > 1.999f && exp2F(1.0f) < 2.001f);
    static_assert(sinD(0.0) == 0.0);
    static_assert(tanhF(20.0f) == 1.0f);
    CHECK(true);
}
