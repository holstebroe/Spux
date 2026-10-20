// Saturator policies, first-order ADAA and the implicit solvers. The alias test is the one the guidelines
// ask of every nonlinear block: a driven sine through each shape, plain, with ADAA, oversampled 4x, and
// both.
#include <doctest.h>

#include <cmath>
#include <vector>

#include "dsp_support.hpp"
#include "spux/dsp/Halfband.hpp"
#include "spux/dsp/Saturator.hpp"

using namespace spux::dsp;

static_assert(AdaaSaturator<TanhSaturator, float>);
static_assert(AdaaSaturator<AsymmetricTanhSaturator, double>);
static_assert(AdaaSaturator<HardClipSaturator, float>);
static_assert(AdaaSaturator<PadeSaturator, float>);
static_assert(ImplicitSolver<NewtonSolver<TanhSaturator, 5>, float>);
static_assert(ImplicitSolver<FixedPointSolver<PadeSaturator, 4>, double>);

namespace {
template <class S> void derivativesAgree() {
    // antiderivative' = value and value' = deriv, by central differences
    for (double x = -5.0; x <= 5.0; x += 0.0173) {
        const double h = 1e-4;
        const double dF = (S::antiderivative(x + h) - S::antiderivative(x - h)) / (2 * h);
        CHECK(dF == doctest::Approx(S::template value<double>(x)).epsilon(2e-3));
        const double dv = (S::template value<double>(x + h) - S::template value<double>(x - h)) / (2 * h);
        if (std::fabs(std::fabs(x) - 1.0) > 2e-4 && std::fabs(x) > 2e-4) // hard clip and asymmetric kinks
            CHECK(dv == doctest::Approx(S::template deriv<double>(x)).epsilon(1e-4).scale(1e-3));
    }
    CHECK(std::fabs(S::antiderivative(0.0)) < 1e-7); // log1pUnit's error; ADAA only uses differences
}

struct AliasResult {
    double plain, adaa, os4, os4adaa;
};

template <class S> AliasResult aliasOf(float drive) {
    const size_t n = 1 << 13, k = 601; // 3.52 kHz at 48 kHz
    auto render = [&](bool adaa, int factor) {
        Adaa1<S> a;
        a.reset();
        Oversampler<float> os;
        os.setFactor(factor);
        std::vector<float> y(3 * n);
        for (size_t i = 0; i < y.size(); ++i) {
            const auto x =
                drive * static_cast<float>(spux::math::sinD(2.0 * spux::math::kPi * static_cast<double>(k) * i / n));
            y[i] = os.process(x, [&](float v) { return adaa ? a.process(v) : S::template value<float>(v); });
        }
        return std::vector<float>(y.end() - static_cast<long>(n), y.end());
    };
    return {spux_test::aliasDb(render(false, 1), k), spux_test::aliasDb(render(true, 1), k),
            spux_test::aliasDb(render(false, 4), k), spux_test::aliasDb(render(true, 4), k)};
}

template <class S> void aliasTest(const char* name, double minAdaaGain, double minOsGain) {
    const auto r = aliasOf<S>(6.0f);
    MESSAGE(std::string(name) << " alias/harmonic power: plain " << r.plain << " dB, ADAA " << r.adaa << ", 4x "
                              << r.os4 << ", 4x+ADAA " << r.os4adaa);
    CHECK(r.adaa < r.plain - minAdaaGain);
    CHECK(r.os4 < r.plain - minOsGain);
    CHECK(r.os4adaa < r.os4);
}
} // namespace

TEST_CASE("antiderivatives and derivatives match the shapes") {
    derivativesAgree<TanhSaturator>();
    derivativesAgree<AsymmetricTanhSaturator>();
    derivativesAgree<HardClipSaturator>();
    derivativesAgree<PadeSaturator>();
}

TEST_CASE("float values agree with double") {
    for (float x = -6.0f; x <= 6.0f; x += 0.01f) {
        CHECK(std::fabs(TanhSaturator::value(x) - std::tanh(static_cast<double>(x))) < 1e-6);
        CHECK(std::fabs(AsymmetricTanhSaturator::value(x) - AsymmetricTanhSaturator::value(static_cast<double>(x))) <
              2e-6);
        CHECK(TanhSaturator::value(static_cast<double>(x)) ==
              doctest::Approx(std::tanh(static_cast<double>(x))).epsilon(1e-14));
    }
}

TEST_CASE("ADAA: small steps fall back to the midpoint, large steps are the difference quotient") {
    Adaa1<HardClipSaturator, double> a;
    a.reset(0.0);
    CHECK(a.process(0.5) == doctest::Approx(0.25));     // (0.125 - 0) / 0.5
    CHECK(a.process(0.501) == doctest::Approx(0.5005)); // midpoint
    CHECK(a.process(3.0) == doctest::Approx((2.5 - 0.5 * 0.501 * 0.501) / (3.0 - 0.501)));
}

TEST_CASE("alias test of every saturator (plain, ADAA, 4x, 4x + ADAA)") {
    // Measured (gcc 13, x64): ADAA alone about 5.5 dB less alias power, 4x about 32 to 44 dB, 4x + ADAA
    // 0.2 dB (smooth shapes) to 12 dB (hard clip) more.
    aliasTest<TanhSaturator>("tanh", 4.0, 25.0);
    aliasTest<AsymmetricTanhSaturator>("asymmetric tanh", 4.0, 25.0);
    aliasTest<HardClipSaturator>("hard clip", 4.0, 25.0);
    aliasTest<PadeSaturator>("pade", 4.0, 25.0);
}

TEST_CASE("Newton and fixed-point solvers solve y + G S(k y) = f0") {
    for (float f0 : {-2.0f, -0.3f, 0.0f, 0.7f, 3.0f}) {
        const float G = 0.6f, k = 1.5f;
        const float y = NewtonSolver<TanhSaturator, 8>::solve(f0, G, k, 0.0f);
        CHECK(std::fabs(y + G * TanhSaturator::value(k * y) - f0) < 1e-5f);
        const float yf = FixedPointSolver<TanhSaturator, 60>::solve(f0, 0.4f, 1.0f, 0.0f);
        CHECK(std::fabs(yf + 0.4f * TanhSaturator::value(yf) - f0) < 1e-5f);
        const double yd = NewtonSolver<AsymmetricTanhSaturator, 8>::solve<double>(f0, 2.0, 1.0, 0.0);
        CHECK(std::fabs(yd + 2.0 * AsymmetricTanhSaturator::value(yd) - f0) < 1e-6);
    }
}
