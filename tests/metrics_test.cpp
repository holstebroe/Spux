// Metrics moved from Tearwash: filters, FFT, band levels and Schroeder decay on signals with known answers.
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "spux/testing/Metrics.hpp"

using namespace spux::testing;

namespace {
// White noise from a fixed generator, so the test is deterministic.
double noise(std::uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return static_cast<double>(s >> 8) / 8388608.0 - 1.0;
}
} // namespace

TEST_CASE("the spectrum of a sine peaks at its frequency") {
    const double fs = 48000.0;
    std::vector<double> x(8192);
    for (std::size_t i = 0; i < x.size(); ++i)
        x[i] = std::sin(2.0 * kMPi * 3000.0 * static_cast<double>(i) / fs);
    const std::vector<double> p = powerSpectrum(x, 0, x.size());
    const std::size_t peak = static_cast<std::size_t>(std::max_element(p.begin(), p.end()) - p.begin());
    CHECK(static_cast<double>(peak) * fs / 8192.0 == doctest::Approx(3000.0).epsilon(0.002));
}

TEST_CASE("a band-pass keeps the band and removes what is far outside it") {
    const double fs = 48000.0;
    std::vector<double> in(16384), out;
    auto rms = [](const std::vector<double>& v) {
        double s = 0;
        for (std::size_t i = 4096; i < v.size() - 4096; ++i)
            s += v[i] * v[i];
        return std::sqrt(s / static_cast<double>(v.size() - 8192));
    };
    for (double f : {250.0, 1000.0, 8000.0}) {
        for (std::size_t i = 0; i < in.size(); ++i)
            in[i] = std::sin(2.0 * kMPi * f * static_cast<double>(i) / fs);
        out = bandpass(in, 700.0, 1400.0, fs);
        const double gain = rms(out) / rms(in);
        if (f == 1000.0)
            CHECK(gain == doctest::Approx(0.9).epsilon(0.15)); // two 4th-order filters, one of them at its corner-ish
        else
            CHECK(gain < 0.05);
    }
}

TEST_CASE("band levels of white noise are flat") {
    std::uint32_t s = 1;
    std::vector<double> x(1 << 16);
    for (double& v : x)
        v = noise(s);
    const std::vector<double> lv = bandLevels(powerSpectrum(x, 0, x.size()), 48000.0, octaveBands());
    for (double v : lv)
        CHECK(std::fabs(v - lv[3]) < 2.0);
}

TEST_CASE("Schroeder decay times of a noise burst with a known RT60") {
    const double fs = 48000.0, rt60 = 1.2;
    std::uint32_t s = 7;
    std::vector<double> h(static_cast<std::size_t>(fs * 3.0));
    for (std::size_t i = 0; i < h.size(); ++i)
        h[i] = noise(s) * std::pow(10.0, -3.0 * static_cast<double>(i) / fs / rt60); // -60 dB after rt60
    const DecayFit f = schroeder(h, 0, fs);
    CHECK(f.rangeDb > 40.0);
    CHECK(f.t20 == doctest::Approx(rt60).epsilon(0.08));
    CHECK(f.t30 == doctest::Approx(rt60).epsilon(0.08));
    CHECK(f.edt == doctest::Approx(rt60).epsilon(0.15));
    CHECK(f.rt() == f.t30);
}

TEST_CASE("a signal without a decay gives no decay time") {
    const DecayFit f = schroeder(std::vector<double>(100, 0.0), 0, 48000.0);
    CHECK(f.rt() == 0.0);
}
