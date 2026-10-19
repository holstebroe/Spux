// BS.1770 loudness: known values, and agreement with the Tannhauser and Plumeflow code this was merged from.
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "spux/testing/Loudness.hpp"

using namespace spux::testing;

namespace {
std::vector<float> sine(double hz, double peakDb, double seconds, int rate = 48000) {
    std::vector<float> x(static_cast<std::size_t>(seconds * rate));
    const double a = std::pow(10.0, peakDb / 20.0);
    for (std::size_t i = 0; i < x.size(); ++i)
        x[i] = static_cast<float>(a * std::sin(2.0 * 3.14159265358979323846 * hz * static_cast<double>(i) / rate));
    return x;
}

// Tannhauser tests/Loudness.hpp (KWeight and maxMomentaryLufs) as they were when the code moved.
struct TannhauserKWeight {
    double x1[2]{}, x2[2]{}, y1[2]{}, y2[2]{};
    double tick(double x) {
        static const double b[2][3] = {{1.53512485958697, -2.69169618940638, 1.19839281085285}, {1.0, -2.0, 1.0}};
        static const double a[2][3] = {{1.0, -1.69065929318241, 0.73248077421585},
                                       {1.0, -1.99004745483398, 0.99007225036621}};
        for (int s = 0; s < 2; ++s) {
            const double y = b[s][0] * x + b[s][1] * x1[s] + b[s][2] * x2[s] - a[s][1] * y1[s] - a[s][2] * y2[s];
            x2[s] = x1[s];
            x1[s] = x;
            y2[s] = y1[s];
            y1[s] = y;
            x = y;
        }
        return x;
    }
};
double tannhauserMaxMomentary(const std::vector<float>& l, const std::vector<float>& r) {
    const int sr = 48000, win = sr * 4 / 10, hop = sr / 10;
    TannhauserKWeight kl, kr;
    std::vector<double> e(l.size());
    for (size_t i = 0; i < l.size(); ++i) {
        const double a = kl.tick(l[i]), b = kr.tick(r[i]);
        e[i] = a * a + b * b;
    }
    double best = 1e-12;
    for (size_t start = 0; start + win <= e.size(); start += hop) {
        double s = 0.0;
        for (int i = 0; i < win; ++i)
            s += e[start + i];
        best = std::max(best, s / win);
    }
    return -0.691 + 10.0 * std::log10(best);
}
} // namespace

TEST_CASE("a 997 Hz sine in both channels at -23 dBFS peak reads -23 LUFS") {
    // BS.1770: 0 dBFS peak in one channel is -3.01 LKFS, so both channels add 3.01 dB.
    const auto x = sine(997.0, -23.0, 10.0);
    const LoudnessResult r = measureLoudness(x, x);
    CHECK(r.finite);
    CHECK(r.integratedLufs == doctest::Approx(-23.0).epsilon(0.002)); // within ~0.05 LU
    CHECK(r.peakDb == doctest::Approx(-23.0).epsilon(0.001));
    CHECK(maxMomentaryLufs(x, x) == doctest::Approx(-23.0).epsilon(0.002));
}

TEST_CASE("silence, short signals and non-finite samples") {
    const std::vector<float> zero(48000 * 2, 0.f);
    CHECK(measureLoudness(zero, zero).integratedLufs == -100.0);
    CHECK(measureLoudness(zero, zero).peakDb == -200.0);
    CHECK(maxMomentaryLufs(zero, zero) == doctest::Approx(-120.691).epsilon(1e-4));
    const std::vector<float> shortSig(1000, 0.5f);
    CHECK(maxMomentaryLufs(shortSig, shortSig) == doctest::Approx(-120.691).epsilon(1e-4)); // shorter than 400 ms
    auto x = sine(997.0, -20.0, 2.0);
    x[100] = std::nanf("");
    const LoudnessResult r = measureLoudness(x, x);
    CHECK_FALSE(r.finite);
    CHECK(std::isfinite(r.integratedLufs));
}

TEST_CASE("the gate leaves out a quiet passage") {
    auto loud = sine(997.0, -20.0, 5.0);
    auto quiet = sine(997.0, -70.0, 5.0); // far below the relative gate
    std::vector<float> both = loud;
    both.insert(both.end(), quiet.begin(), quiet.end());
    // the blocks that straddle the change are louder than the gate and stay in: a fraction of a LU
    CHECK(std::fabs(measureLoudness(both, both).integratedLufs - measureLoudness(loud, loud).integratedLufs) < 0.3);
}

TEST_CASE("max momentary loudness matches the Tannhauser implementation") {
    auto l = sine(220.0, -18.0, 3.0);
    auto r = sine(331.0, -21.0, 3.0);
    for (std::size_t i = 0; i < l.size(); ++i)
        l[i] *= 1.0f - static_cast<float>(i) / static_cast<float>(l.size()) * 0.5f; // not stationary
    CHECK(std::fabs(maxMomentaryLufs(l, r) - tannhauserMaxMomentary(l, r)) < 1e-9);
}
