// Noise sources and the Ornstein-Uhlenbeck drift.
#include <doctest.h>

#include <cmath>
#include <vector>

#include "dsp_support.hpp"
#include "spux/dsp/Drift.hpp"
#include "spux/dsp/Noise.hpp"

using namespace spux::dsp;

TEST_CASE("white noise: range, RMS, repeatable from the seed") {
    WhiteNoise<float> a, b;
    a.seed(spux::Pcg32(5, 6));
    b.seed(spux::Pcg32(5, 6));
    double s2 = 0;
    for (int i = 0; i < 100000; ++i) {
        const float x = a.tick();
        REQUIRE(x == b.tick());
        REQUIRE(x >= -1.0f);
        REQUIRE(x < 1.0f);
        s2 += x * x;
    }
    CHECK(std::sqrt(s2 / 100000) == doctest::Approx(1.0 / std::sqrt(3.0)).epsilon(0.01));
}

TEST_CASE("pink noise: -3 dB per octave between 100 Hz and 5 kHz, about 0.43 RMS") {
    PinkNoise<float> p;
    p.seed(spux::Pcg32(11, 12));
    const size_t n = 1 << 20;
    std::vector<double> x(n);
    double s2 = 0;
    for (auto& v : x) {
        v = p.tick();
        s2 += v * v;
    }
    CHECK(std::sqrt(s2 / n) == doctest::Approx(0.43).epsilon(0.05));
    // averaged power spectrum over segments
    const size_t seg = 1 << 13;
    std::vector<double> avg(seg / 2 + 1, 0.0);
    for (size_t s = 0; s + seg <= n; s += seg) {
        const auto pw = spux::testing::powerSpectrum(x, s, s + seg);
        for (size_t k = 0; k < avg.size(); ++k)
            avg[k] += pw[k];
    }
    auto bandDb = [&](double lo, double hi) {
        double sum = 0;
        int cnt = 0;
        for (size_t k = 0; k < avg.size(); ++k) {
            const double f = k * spux_test::kFs / seg;
            if (f >= lo && f < hi) {
                sum += avg[k];
                ++cnt;
            }
        }
        return 10.0 * std::log10(sum / cnt); // power density
    };
    for (double f = 100.0; f < 5000.0; f *= 2.0) {
        const double slope = bandDb(2 * f, 2 * f * 1.2) - bandDb(f, f * 1.2);
        INFO("octave from " << f << " Hz: " << slope << " dB");
        CHECK(slope == doctest::Approx(-3.01).epsilon(0.35)); // within about 1 dB
    }
}

TEST_CASE("OU drift: stationary RMS sigma for any step size, repeatable") {
    for (double dt : {64.0 / 48000.0, 1.0 / 48000.0 * 512}) {
        OuDrift<double> d;
        d.seed(spux::Pcg32(21, 22));
        d.setup(10.0, 0.5, dt);
        d.reset();
        double s2 = 0;
        const int n = 400000;
        for (int i = 0; i < n; ++i) {
            const double v = d.step();
            s2 += v * v;
        }
        INFO("dt " << dt);
        CHECK(std::sqrt(s2 / n) == doctest::Approx(10.0).epsilon(0.05));
    }
    OuDrift<float> a, b;
    a.seed(spux::Pcg32(1, 1));
    b.seed(spux::Pcg32(1, 1));
    a.setup(1.0, 2.0, 0.01);
    b.setup(1.0, 2.0, 0.01);
    a.reset();
    b.reset();
    for (int i = 0; i < 100; ++i)
        CHECK(a.step() == b.step());
}
