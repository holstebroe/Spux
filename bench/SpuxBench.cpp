// spux_bench: speed of the spux math against libm (R2, S4: spux math replaces libm in the audio path only
// where it is at least as fast) and of the DSP blocks, in ns per call or per sample. Release builds only;
// the results are recorded in docs/BENCH.md. Usage: spux_bench [--quick]
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "spux/core/Math.hpp"
#include "spux/core/Pcg32.hpp"
#include "spux/core/Smoothing.hpp"
#include "spux/dsp/Biquad.hpp"
#include "spux/dsp/Blep.hpp"
#include "spux/dsp/DelayLine.hpp"
#include "spux/dsp/Halfband.hpp"
#include "spux/dsp/Noise.hpp"
#include "spux/dsp/OnePole.hpp"
#include "spux/dsp/Saturator.hpp"
#include "spux/dsp/Svf.hpp"

namespace {

int gReps = 20;
volatile double gSink = 0.0;

// Best of gReps runs of f over the input, in ns per element.
template <class T, class F> double timeIt(const std::vector<T>& in, F&& f) {
    double best = 1e30;
    for (int r = 0; r < gReps; ++r) {
        const auto t0 = std::chrono::steady_clock::now();
        double acc = 0.0;
        for (const T& v : in)
            acc += static_cast<double>(f(v));
        const auto t1 = std::chrono::steady_clock::now();
        gSink = gSink + acc;
        best =
            std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(in.size()));
    }
    return best;
}

template <class T> std::vector<T> uniform(size_t n, double lo, double hi) {
    spux::Pcg32 rng(1, 2);
    std::vector<T> v(n);
    for (auto& x : v)
        x = static_cast<T>(lo + (hi - lo) * rng.nextDouble());
    return v;
}

void row(const char* name, double spux, double libm) {
    if (libm > 0.0)
        std::printf("| %-34s | %7.2f | %7.2f | %5.2f |\n", name, spux, libm, libm / spux);
    else
        std::printf("| %-34s | %7.2f |       - |     - |\n", name, spux);
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--quick") == 0)
        gReps = 3;
#ifndef NDEBUG
    std::printf("warning: not a Release build; the numbers mean nothing\n");
#endif
    using namespace spux;
    const size_t n = 1 << 16;
    const auto f60 = uniform<float>(n, -60.0, 60.0);
    const auto f8 = uniform<float>(n, -8.0, 8.0);
    const auto fTan = uniform<float>(n, 1e-3, 1.5);
    const auto fPos = uniform<float>(n, 1e-3, 1e3);
    const auto d40 = uniform<double>(n, -40.0, 40.0);
    const auto dPos = uniform<double>(n, 1e-3, 1e4);
    const auto dTan = uniform<double>(n, -1.5, 1.5);
    const auto dTurns = uniform<double>(n, -100.0, 100.0);

    std::printf("## Math (ns per call; speed-up = libm / spux)\n\n");
    std::printf("| %-34s | %7s | %7s | %5s |\n", "function", "spux", "libm", "x");
    std::printf("| --- | ---: | ---: | ---: |\n");
    row("exp2F (std::exp2 float)", timeIt(f60, [](float x) { return math::exp2F(x); }),
        timeIt(f60, [](float x) { return std::exp2(x); }));
    row("fastTanhF (std::tanh float)", timeIt(f8, [](float x) { return math::fastTanhF(x); }),
        timeIt(f8, [](float x) { return std::tanh(x); }));
    row("tanhPade76 (std::tanh float)", timeIt(f8, [](float x) { return math::tanhPade76(x); }),
        timeIt(f8, [](float x) { return std::tanh(x); }));
    row("tanhPade32 (std::tanh float)", timeIt(f8, [](float x) { return math::tanhPade32(x); }),
        timeIt(f8, [](float x) { return std::tanh(x); }));
    row("sinTurns (std::sin float)", timeIt(dTurns, [](double t) { return math::sinTurns(t); }),
        timeIt(dTurns, [](double t) { return std::sin(static_cast<float>(t * 6.283185307179586)); }));
    row("tanRad (std::tan float)", timeIt(fTan, [](float x) { return math::tanRad(x); }),
        timeIt(fTan, [](float x) { return std::tan(x); }));
    row("log2Approx (std::log2 float)", timeIt(fPos, [](float x) { return math::log2Approx(x); }),
        timeIt(fPos, [](float x) { return std::log2(x); }));
    row("atanApprox (std::atan float)", timeIt(f8, [](float x) { return math::atanApprox(x); }),
        timeIt(f8, [](float x) { return std::atan(x); }));
    row("logCosh (log(cosh) double)", timeIt(d40, [](double x) { return math::logCosh(x); }),
        timeIt(d40, [](double x) { return std::log(std::cosh(x)); }));
    row("dbToGainF (std::pow float)", timeIt(f60, [](float x) { return math::dbToGainF(x); }),
        timeIt(f60, [](float x) { return std::pow(10.0f, x / 20.0f); }));
    row("expD (std::exp)", timeIt(d40, [](double x) { return math::expD(x); }),
        timeIt(d40, [](double x) { return std::exp(x); }));
    row("exp2D (std::exp2)", timeIt(d40, [](double x) { return math::exp2D(x); }),
        timeIt(d40, [](double x) { return std::exp2(x); }));
    row("logD (std::log)", timeIt(dPos, [](double x) { return math::logD(x); }),
        timeIt(dPos, [](double x) { return std::log(x); }));
    row("sinD (std::sin)", timeIt(d40, [](double x) { return math::sinD(x); }),
        timeIt(d40, [](double x) { return std::sin(x); }));
    row("tanD (std::tan)", timeIt(dTan, [](double x) { return math::tanD(x); }),
        timeIt(dTan, [](double x) { return std::tan(x); }));

    std::printf("\n## Blocks (ns per host-rate sample, float, 48 kHz)\n\n");
    std::printf("| %-34s | %7s | %7s | %5s |\n", "block", "ns", "", "");
    std::printf("| --- | ---: | ---: | ---: |\n");
    const auto x = uniform<float>(n, -1.0, 1.0);
    {
        dsp::OnePoleTpt<float> f;
        f.setCutoff(1000.0, 48000.0);
        row("OnePoleTpt lowpass", timeIt(x, [&](float v) { return f.lowpass(v); }), 0);
    }
    {
        dsp::Svf<float> f;
        f.setCutoff(1000.0, 2.0, 48000.0);
        row("Svf tick", timeIt(x, [&](float v) { return f.tick(v).low; }), 0);
    }
    {
        dsp::Biquad<float> f;
        f.set(dsp::designLowpass(1000.0, 0.7, 48000.0));
        row("Biquad tick", timeIt(x, [&](float v) { return f.tick(v); }), 0);
    }
    {
        dsp::DelayLine<float> d;
        d.prepare(4096);
        double t = 0;
        row("DelayLine write + readHermite",
            timeIt(x,
                   [&](float v) {
                       d.write(v);
                       t += 0.01;
                       return d.readHermite(1000.5 + (t - static_cast<double>(static_cast<long>(t))));
                   }),
            0);
    }
    for (int factor : {2, 4, 8}) {
        dsp::Oversampler<float> os;
        os.setFactor(factor);
        const std::string name = "Oversampler x" + std::to_string(factor) + " round trip";
        row(name.c_str(), timeIt(x, [&](float v) { return os.process(v, [](float u) { return u; }); }), 0);
    }
    {
        dsp::Adaa1<dsp::TanhSaturator> a;
        a.reset();
        row("Adaa1<TanhSaturator>", timeIt(x, [&](float v) { return a.process(4.0f * v); }), 0);
        row("TanhSaturator::value", timeIt(x, [&](float v) { return dsp::TanhSaturator::value(4.0f * v); }), 0);
    }
    {
        dsp::Oversampler<float> os;
        os.setFactor(4);
        dsp::Adaa1<dsp::TanhSaturator> a;
        a.reset();
        row("Oversampler x4 + Adaa1 tanh",
            timeIt(x, [&](float v) { return os.process(4.0f * v, [&](float u) { return a.process(u); }); }), 0);
    }
    row("Blep<16>::residual", timeIt(x, [](float v) { return dsp::Blep<16>::residual(v * 8.0f); }), 0);
    {
        dsp::PinkNoise<float> p;
        p.seed(Pcg32(1, 1));
        row("PinkNoise tick", timeIt(x, [&](float) { return p.tick(); }), 0);
    }
    {
        Smoothed<OnePole<0.01>> s;
        s.prepare(48000.0);
        int i = 0;
        row("Smoothed<OnePole> next",
            timeIt(x,
                   [&](float v) {
                       if ((++i & 63) == 0)
                           s.setTarget(v);
                       return s.next();
                   }),
            0);
    }
    std::printf("\n(sink %g)\n", static_cast<double>(gSink));
    return 0;
}
