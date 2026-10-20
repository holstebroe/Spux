// BLEP residuals: shape properties and the aliasing of a band-limited sawtooth per order, and the
// pt2-clone minimum-phase BLEP.
#include <doctest.h>

#include <cmath>
#include <vector>

#include "dsp_support.hpp"
#include "spux/dsp/Blep.hpp"
#include "spux/dsp/pt2/MinBlep.hpp"

using namespace spux::dsp;

namespace {
template <int Order> void residualShape() {
    using B = Blep<Order, double>;
    CHECK(B::residual(-B::kWidth) == 0.0);
    CHECK(B::residual(B::kWidth) == 0.0);
    CHECK(B::residual(-0.0001) == doctest::Approx(0.5).epsilon(0.01));
    CHECK(B::residual(0.0001) == doctest::Approx(-0.5).epsilon(0.01));
    CHECK(B::residual(-0.37) == doctest::Approx(-B::residual(0.37)).epsilon(1e-6)); // odd
}

// A sawtooth from -1 to 1 whose fundamental sits on bin k of n samples; returns its alias ratio (dB).
template <int Order> double sawAlias(bool corrected) {
    const size_t n = 1 << 14, k = 1001; // about 2.9 kHz at 48 kHz, 16.37 samples per period
    const double inc = static_cast<double>(k) / n;
    const int width = static_cast<int>(Blep<Order, double>::kWidth);
    std::vector<double> naive(n);
    // naive saw, plus the residual around every wrap (periodic indices)
    for (size_t i = 0; i < n; ++i) {
        const double ph = std::fmod(i * inc, 1.0);
        naive[i] = 2.0 * ph - 1.0;
    }
    std::vector<double> out = naive;
    if (corrected) {
        for (size_t wrap = 1; wrap < k + 1; ++wrap) {
            const double t = wrap / inc; // exact time of the wrap in samples
            const auto i0 = static_cast<long>(std::floor(t));
            for (long j = i0 - width + 1; j <= i0 + width; ++j) {
                const double x = static_cast<double>(j) - t;
                const long idx = ((j % static_cast<long>(n)) + static_cast<long>(n)) % static_cast<long>(n);
                out[static_cast<size_t>(idx)] += -2.0 * Blep<Order, double>::residual(x);
            }
        }
    }
    return spux_test::aliasDb(spux_test::toFloat(out), k);
}
} // namespace

TEST_CASE("residuals: zero outside the support, -0.5 jump at the step, odd symmetry") {
    residualShape<2>();
    residualShape<4>();
    residualShape<16>();
    residualShape<32>();
    CHECK(Blep<4, float>::residual(1.5f) == doctest::Approx(-std::pow(0.5f, 4.0f) / 24.0f));
}

TEST_CASE("alias reduction of a sawtooth per order") {
    const double naive = sawAlias<2>(false);
    const double o2 = sawAlias<2>(true), o4 = sawAlias<4>(true), o16 = sawAlias<16>(true);
    MESSAGE("saw alias/harmonic power: naive " << naive << " dB, order 2 " << o2 << ", order 4 " << o4 << ", order 16 "
                                               << o16);
    CHECK(o2 < naive - 10.0);
    CHECK(o4 < o2 - 3.0);
    CHECK(o16 < o4 - 10.0);
}

TEST_CASE("pt2 MinBlep: a corrected square has less aliasing, and the correction decays to zero") {
    const size_t n = 1 << 14, k = 1001;
    const double inc = static_cast<double>(k) / n;
    auto render = [&](bool corrected) {
        pt2::MinBlep<float> blep;
        blep.reset();
        std::vector<float> y(3 * n);
        double phase = 0.0;
        float level = 1.0f;
        for (size_t i = 0; i < y.size(); ++i) {
            phase += inc;
            if (phase >= 0.5 && level > 0.0f) {
                const double off = (phase - 0.5) / inc; // samples since the edge
                if (corrected)
                    blep.add(static_cast<float>(off), 2.0f); // samples since the edge; old - new = 1 - (-1)
                level = -1.0f;
            } else if (phase >= 1.0) {
                phase -= 1.0;
                const double off = phase / inc;
                if (corrected)
                    blep.add(static_cast<float>(off), -2.0f);
                level = 1.0f;
            }
            y[i] = blep.run(level);
        }
        return std::vector<float>(y.end() - static_cast<long>(n), y.end());
    };
    const double naive = spux_test::aliasDb(render(false), k);
    const double minblep = spux_test::aliasDb(render(true), k);
    MESSAGE("square alias/harmonic power: naive " << naive << " dB, pt2 MinBlep " << minblep << " dB");
    CHECK(minblep < naive - 10.0);

    pt2::MinBlep<float> b;
    b.reset();
    b.add(0.3f, 1.0f);
    CHECK(b.samplesLeft() == pt2::kBlepSamples);
    float last = 1.0f;
    for (int i = 0; i < 40; ++i)
        last = b.run(0.0f);
    CHECK(last == 0.0f);
}
