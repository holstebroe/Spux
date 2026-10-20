// Half-band stage and oversampler: the measured stopband and passband of the chosen design (the numbers
// behind the 5+5 choice, docs/BENCH.md), the cascade's image rejection and the API forms.
#include <doctest.h>

#include <cmath>
#include <complex>
#include <vector>

#include "dsp_support.hpp"
#include "spux/dsp/Halfband.hpp"

using namespace spux::dsp;

namespace {
// Impulse response of one 2x interpolation stage at the high rate (gain-normalized to 1 at DC).
std::vector<std::complex<double>> stageSpectrum(size_t n) {
    HalfbandStage<float> st;
    std::vector<std::complex<double>> h(n);
    for (size_t i = 0; i < n / 2; ++i) {
        float a, b;
        st.upsample(i == 0 ? 1.0f : 0.0f, a, b);
        h[2 * i] = 0.5 * a;
        h[2 * i + 1] = 0.5 * b;
    }
    spux::testing::fft(h);
    return h;
}
} // namespace

TEST_CASE("half-band stage: stopband below -105 dB, passband flat (float implementation)") {
    const size_t n = 1 << 16;
    const auto h = stageSpectrum(n);
    double passMax = -1e9, passMin = 1e9, stop = -1e9;
    for (size_t k = 0; k <= n / 2; ++k) {
        const double f = static_cast<double>(k) / static_cast<double>(n); // of the high rate
        const double db = 20.0 * std::log10(std::abs(h[k]) + 1e-300);
        if (f <= 0.225) {
            passMax = std::max(passMax, db);
            passMin = std::min(passMin, db);
        }
        if (f >= 0.275)
            stop = std::max(stop, db);
    }
    MESSAGE("passband " << passMin << " .. " << passMax << " dB, stopband max " << stop << " dB");
    CHECK(passMax - passMin < 1e-4);
    CHECK(stop < -105.0);
}

TEST_CASE("4x cascade rejects the images of a sine") {
    Oversampler<float> os;
    os.setFactor(4);
    const size_t L = 1 << 14;
    std::vector<double> y;
    y.reserve(4 * L);
    for (size_t i = 0; i < L + 4096; ++i) {
        float out[8];
        os.upsample(0.5f * static_cast<float>(std::sin(2.0 * spux::math::kPi * 1000.0 / spux_test::kFs * i)), out);
        if (i >= 4096)
            for (int k = 0; k < 4; ++k)
                y.push_back(out[k]);
    }
    // Blackman-Harris window, then the worst component away from the tone.
    std::vector<std::complex<double>> a(y.size());
    for (size_t j = 0; j < y.size(); ++j) {
        const double ph = 2.0 * spux::math::kPi * (j + 0.5) / y.size();
        a[j] = y[j] * (0.35875 - 0.48829 * std::cos(ph) + 0.14128 * std::cos(2 * ph) - 0.01168 * std::cos(3 * ph));
    }
    spux::testing::fft(a);
    double peak = 0.0, spur = 0.0;
    size_t pk = 0;
    for (size_t k = 0; k < a.size() / 2; ++k)
        if (std::abs(a[k]) > peak) {
            peak = std::abs(a[k]);
            pk = k;
        }
    for (size_t k = 0; k < a.size() / 2; ++k)
        if (k + 300 < pk || k > pk + 300)
            spur = std::max(spur, std::abs(a[k]));
    const double db = 20.0 * std::log10(spur / peak);
    MESSAGE("worst spurious component " << db << " dB");
    CHECK(db < -85.0); // window-limited; a split second stage (Plumeflow's 4x) measures about -36 dB
}

TEST_CASE("oversampler: factors, round trip, process() and block forms agree") {
    Oversampler<float> os;
    os.setFactor(3);
    CHECK(os.factor() == 4);
    os.setFactor(16);
    CHECK(os.factor() == 8);
    os.setFactor(1);
    CHECK(os.factor() == 1);
    float one[8];
    os.upsample(0.25f, one);
    CHECK(one[0] == 0.25f);
    CHECK(os.downsample(one) == 0.25f);

    for (int factor : {2, 4, 8}) {
        Oversampler<float> a, b, c;
        a.setFactor(factor);
        b.setFactor(factor);
        c.setFactor(factor);
        const auto x = spux_test::stimulus(512);
        std::vector<float> upBlock(x.size() * static_cast<size_t>(factor)), viaBlock(x.size());
        c.up(x.data(), static_cast<int>(x.size()), upBlock.data());
        for (auto& v : upBlock)
            v = std::tanh(2.0f * v);
        c.down(upBlock.data(), static_cast<int>(x.size()), viaBlock.data());
        for (size_t i = 0; i < x.size(); ++i) {
            float buf[8];
            a.upsample(x[i], buf);
            for (int k = 0; k < factor; ++k)
                buf[k] = std::tanh(2.0f * buf[k]);
            const float manual = a.downsample(buf);
            const float viaProcess = b.process(x[i], [](float v) { return std::tanh(2.0f * v); });
            REQUIRE(manual == viaProcess);
            REQUIRE(manual == viaBlock[i]);
        }
    }
}

TEST_CASE("a low tone passes the 2x round trip with unit gain and a small delay") {
    Oversampler<double> os;
    os.setFactor(2);
    double inPow = 0, outPow = 0, cross = 0;
    for (int i = 0; i < 48000; ++i) {
        const double x = std::sin(2.0 * spux::math::kPi * 1000.0 * i / spux_test::kFs);
        const double y = os.process(x, [](double v) { return v; });
        if (i > 1000) {
            inPow += x * x;
            outPow += y * y;
            cross += x * y;
        }
    }
    CHECK(std::sqrt(outPow / inPow) == doctest::Approx(1.0).epsilon(1e-4)); // partial periods at the ends
    // correlation with the input: a delay of d samples at 1 kHz gives cos(2 pi 1000 d / fs)
    const double delay =
        std::acos(cross / std::sqrt(inPow * outPow)) * spux_test::kFs / (2.0 * spux::math::kPi * 1000.0);
    MESSAGE("2x round trip delay at 1 kHz: " << delay << " samples");
    CHECK(delay < 4.0);
}
