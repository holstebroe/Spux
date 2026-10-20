// Linear blocks: one-pole TPT, SVF, biquad designs, DC blocker and delay line reads, checked against
// their stated laws (responses at the cutoff, interpolation exactness).
#include <doctest.h>

#include <cmath>
#include <complex>
#include <vector>

#include "dsp_support.hpp"
#include "spux/dsp/Biquad.hpp"
#include "spux/dsp/DcBlock.hpp"
#include "spux/dsp/DelayLine.hpp"
#include "spux/dsp/OnePole.hpp"
#include "spux/dsp/Svf.hpp"

using namespace spux::dsp;
using spux_test::kFs;

namespace {
// |H(f)| in dB from an impulse response of n samples.
template <class F> double responseDb(F&& tick, double f, int n = 1 << 15) {
    std::complex<double> acc = 0.0;
    for (int i = 0; i < n; ++i) {
        const double h = tick(i == 0 ? 1.0 : 0.0);
        acc += h * std::polar(1.0, -2.0 * spux::math::kPi * f / kFs * i);
    }
    return 20.0 * std::log10(std::abs(acc));
}
} // namespace

TEST_CASE("one-pole TPT: -3 dB at the cutoff, complementary high-pass, unit all-pass") {
    OnePoleTpt<double> lp, hp, ap;
    for (auto* f : {&lp, &hp, &ap})
        f->setCutoff(1000.0, kFs);
    CHECK(responseDb([&](double x) { return lp.lowpass(x); }, 1000.0) == doctest::Approx(-3.0103).epsilon(1e-3));
    CHECK(responseDb([&](double x) { return hp.highpass(x); }, 1000.0) == doctest::Approx(-3.0103).epsilon(1e-3));
    CHECK(responseDb([&](double x) { return ap.allpass(x); }, 5000.0) == doctest::Approx(0.0).epsilon(1e-6));
    OnePoleTpt<float> clamp;
    clamp.setCutoff(1e9, kFs);
    CHECK(clamp.G() < 1.0f);
}

TEST_CASE("SVF: Butterworth low and high at the cutoff, resonance peak, notch") {
    Svf<double> s;
    s.setCutoff(2000.0, 0.7071067811865476, kFs);
    CHECK(responseDb([&](double x) { return s.tick(x).low; }, 2000.0) == doctest::Approx(-3.0103).epsilon(1e-3));
    s.reset();
    CHECK(responseDb([&](double x) { return s.tick(x).high; }, 2000.0) == doctest::Approx(-3.0103).epsilon(1e-3));
    Svf<double> r;
    r.setCutoff(2000.0, 4.0, kFs);
    CHECK(responseDb([&](double x) { return r.tick(x).band; }, 2000.0) ==
          doctest::Approx(20.0 * std::log10(4.0)).epsilon(1e-3));
    r.reset();
    CHECK(responseDb([&](double x) { return r.tick(x).bandUnity(); }, 2000.0) == doctest::Approx(0.0).epsilon(1e-6));
    r.reset();
    CHECK(responseDb([&](double x) { return r.tick(x).notch(); }, 2000.0) < -60.0);
    r.reset();
    CHECK(responseDb([&](double x) { return r.tick(x).allpass(); }, 7000.0) == doctest::Approx(0.0).epsilon(1e-6));
}

TEST_CASE("biquad RBJ designs") {
    Biquad<double> b;
    b.set(designLowpass(1000.0, 0.7071067811865476, kFs));
    CHECK(responseDb([&](double x) { return b.tick(x); }, 1000.0) == doctest::Approx(-3.0103).epsilon(1e-3));
    b.reset();
    b.set(designHighpass(1000.0, 0.7071067811865476, kFs));
    CHECK(responseDb([&](double x) { return b.tick(x); }, 1000.0) == doctest::Approx(-3.0103).epsilon(1e-3));
    b.reset();
    b.set(designBandpass(3000.0, 2.0, kFs));
    CHECK(responseDb([&](double x) { return b.tick(x); }, 3000.0) == doctest::Approx(0.0).epsilon(1e-6));
    b.reset();
    b.set(designNotch(3000.0, 2.0, kFs));
    CHECK(responseDb([&](double x) { return b.tick(x); }, 3000.0) < -60.0);
    b.reset();
    b.set(designAllpass(3000.0, 2.0, kFs));
    CHECK(responseDb([&](double x) { return b.tick(x); }, 9000.0) == doctest::Approx(0.0).epsilon(1e-6));
    b.reset();
    b.set(designPeak(3000.0, 1.0, 6.0, kFs));
    CHECK(responseDb([&](double x) { return b.tick(x); }, 3000.0) == doctest::Approx(6.0).epsilon(1e-4));
    b.reset();
    b.set(designLowShelf(500.0, -9.0, kFs));
    CHECK(responseDb([&](double x) { return b.tick(x); }, 20.0) == doctest::Approx(-9.0).epsilon(1e-2));
    CHECK(responseDb([&](double x) { return b.tick(x); }, 15000.0) == doctest::Approx(0.0).epsilon(1e-2));
    b.reset();
    b.set(designHighShelf(4000.0, 6.0, kFs));
    CHECK(responseDb([&](double x) { return b.tick(x); }, 20000.0) == doctest::Approx(6.0).epsilon(2e-2));
}

TEST_CASE("DC blocker removes an offset") {
    DcBlock<float> dc;
    dc.setCutoff(8.0, kFs);
    float y = 0.0f;
    for (int i = 0; i < 48000; ++i)
        y = dc.tick(0.5f);
    CHECK(std::fabs(y) < 1e-3f);
    DcBlock<double> d;
    d.setCutoff(8.0, kFs);
    CHECK(responseDb([&](double x) { return d.tick(x); }, 8.0, 1 << 18) == doctest::Approx(-3.0).epsilon(0.02));
}

TEST_CASE("delay line: integer, linear and Hermite reads") {
    DelayLine<float> d;
    d.prepare(100);
    CHECK(d.maxDelay() >= 100);
    for (int i = 0; i < 200; ++i)
        d.write(static_cast<float>(i));
    CHECK(d.readInt(1) == 199.0f);
    CHECK(d.readInt(10) == 190.0f);
    CHECK(d.readLinear(2.5) == 197.5f);
    CHECK(d.readHermite(3.25) == doctest::Approx(196.75f)); // exact on a ramp
    // Catmull-Rom (finite-difference slopes) reproduces a quadratic exactly.
    DelayLine<double> q;
    q.prepare(64);
    for (int i = 0; i < 64; ++i)
        q.write(0.01 * i * i);
    const double pos = 64.0 - 10.4; // sample index at delay 10.4 (delay 1 is index 63)
    CHECK(q.readHermite(10.4) == doctest::Approx(0.01 * pos * pos).epsilon(1e-12));
    d.clear();
    CHECK(d.readInt(5) == 0.0f);
}
