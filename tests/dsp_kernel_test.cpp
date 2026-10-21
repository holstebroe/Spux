// Kernel references (guidelines section 3, S5): every core and DSP block renders a fixed stimulus; the
// output's golden hash and its first values are stored in tests/kernels/. A changed hash is a change
// detector: if the change is intended, run with SPUX_UPDATE_GOLDEN=1, commit the files with the change
// and add a line to docs/CHANGELOG.md.
#include <doctest.h>

#include <vector>

#include "dsp_support.hpp"
#include "spux/core/ControlTick.hpp"
#include "spux/core/Math.hpp"
#include "spux/core/Pcg32.hpp"
#include "spux/core/Smoothing.hpp"
#include "spux/dsp/Biquad.hpp"
#include "spux/dsp/Blep.hpp"
#include "spux/dsp/Chorus.hpp"
#include "spux/dsp/DcBlock.hpp"
#include "spux/dsp/DelayLine.hpp"
#include "spux/dsp/Drift.hpp"
#include "spux/dsp/Envelope.hpp"
#include "spux/dsp/Halfband.hpp"
#include "spux/dsp/Lfo.hpp"
#include "spux/dsp/Noise.hpp"
#include "spux/dsp/OnePole.hpp"
#include "spux/dsp/Saturator.hpp"
#include "spux/dsp/Svf.hpp"
#include "spux/dsp/pt2/MinBlep.hpp"

using namespace spux;
using namespace spux::dsp;
using spux_test::kFs;

namespace {
template <class F> std::vector<float> map(const std::vector<float>& x, F&& f) {
    std::vector<float> y;
    y.reserve(x.size());
    for (float v : x)
        y.push_back(f(v));
    return y;
}

template <int Order> std::vector<float> blepSaw() {
    // a sawtooth at 1234.5 Hz with the residual added around each wrap
    const int n = 4096;
    const double inc = 1234.5 / kFs;
    std::vector<double> y(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
        y[static_cast<size_t>(i)] = 2.0 * (i * inc - math::floorD(i * inc)) - 1.0;
    constexpr int w = static_cast<int>(Blep<Order>::kWidth);
    for (int wrap = 1; wrap / inc < n + w; ++wrap) {
        const double t = wrap / inc;
        const auto i0 = static_cast<int>(math::floorD(t));
        for (int j = i0 - w + 1; j <= i0 + w; ++j)
            if (j >= 0 && j < n)
                y[static_cast<size_t>(j)] += -2.0 * Blep<Order>::residual(static_cast<float>(j - t));
    }
    return spux_test::toFloat(y);
}

template <class S> std::vector<float> adaaRun(const std::vector<float>& x) {
    Adaa1<S> a;
    a.reset();
    return map(x, [&](float v) { return a.process(4.0f * v); });
}
} // namespace

TEST_CASE("kernel references") {
    spux_test::KernelReferences refs;
    const auto x = spux_test::stimulus();

    // core
    {
        std::vector<float> y;
        for (int i = 0; i < 4096; ++i) {
            const float t = static_cast<float>(i) / 4096.0f;
            y.push_back(math::exp2F(t * 40.0f - 20.0f) * 1e-6f);
            y.push_back(math::fastTanhF(t * 16.0f - 8.0f));
            y.push_back(math::sinTurns(t * 7.3));
            y.push_back(math::tanRad(t * 1.5f + 1e-3f));
            y.push_back(math::tanhPade76(t * 12.0f - 6.0f));
            y.push_back(math::tanhPade32(t * 8.0f - 4.0f));
            y.push_back(static_cast<float>(math::expD(t * 4.0 - 2.0)));
            y.push_back(static_cast<float>(math::logD(t + 0.01)));
            y.push_back(static_cast<float>(math::sinD(t * 50.0)));
            y.push_back(static_cast<float>(math::logCosh(t * 20.0 - 10.0)));
        }
        refs.check("math", y);
    }
    {
        Pcg32 rng(77, 5);
        std::vector<float> y;
        for (int i = 0; i < 2048; ++i) {
            y.push_back(rng.nextFloat());
            y.push_back(static_cast<float>(rng.nextGaussian()));
        }
        auto d = Pcg32::derive(1, 2, 3, 4);
        for (int i = 0; i < 64; ++i)
            y.push_back(static_cast<float>(d.next() >> 8));
        refs.check("pcg32", y);
    }
    {
        Smoothed<LinearRamp<64>> lin;
        Smoothed<OnePole<0.005>> pole;
        Smoothed<Slew<50.0>> slew;
        pole.prepare(kFs);
        slew.prepare(kFs);
        ControlTick<64> tick;
        std::vector<float> y;
        int target = 0;
        tick.run(
            4096,
            [&] {
                const float t = static_cast<float>((target++ * 7) % 5) * 0.25f;
                lin.setTarget(t);
                pole.setTarget(t);
                slew.setTarget(t);
            },
            [&](int, int count) {
                for (int i = 0; i < count; ++i) {
                    y.push_back(lin.next());
                    y.push_back(pole.next());
                    y.push_back(slew.next());
                }
            });
        refs.check("smoothing", y);
    }

    // linear filters
    {
        OnePoleTpt<float> lp, hp, ap;
        for (auto* f : {&lp, &hp, &ap})
            f->setCutoff(1000.0, kFs);
        std::vector<float> y;
        for (float v : x) {
            y.push_back(lp.lowpass(v));
            y.push_back(hp.highpass(v));
            y.push_back(ap.allpass(v));
        }
        refs.check("onepole_tpt", y);
    }
    {
        Svf<float> s;
        s.setCutoff(2000.0, 3.0, kFs);
        std::vector<float> y;
        for (float v : x) {
            const auto o = s.tick(v);
            y.push_back(o.low);
            y.push_back(o.band);
            y.push_back(o.high);
        }
        refs.check("svf", y);
    }
    {
        Biquad<float> lp, pk, hs;
        lp.set(designLowpass(1500.0, 0.9, kFs));
        pk.set(designPeak(3000.0, 2.0, 9.0, kFs));
        hs.set(designHighShelf(6000.0, -6.0, kFs));
        std::vector<float> y;
        for (float v : x) {
            y.push_back(lp.tick(v));
            y.push_back(pk.tick(v));
            y.push_back(hs.tick(v));
        }
        refs.check("biquad", y);
    }
    {
        DcBlock<float> dc;
        dc.setCutoff(8.0, kFs);
        refs.check("dc_block", map(x, [&](float v) { return dc.tick(v + 0.3f); }));
    }
    {
        DelayLine<float> d;
        d.prepare(600);
        std::vector<float> y;
        int i = 0;
        for (float v : x) {
            d.write(v);
            const double delay = 300.0 + 200.0 * math::sinD(i++ * 0.003);
            y.push_back(d.readHermite(delay));
            y.push_back(d.readLinear(delay * 0.5));
            y.push_back(d.readInt(17));
        }
        refs.check("delay_line", y);
    }

    // oversampling and nonlinear blocks
    {
        HalfbandStage<float> up, down;
        std::vector<float> y;
        for (float v : x) {
            float a, b;
            up.upsample(v, a, b);
            y.push_back(a);
            y.push_back(b);
            y.push_back(down.downsample(a * a, b * b));
        }
        refs.check("halfband_stage", y);
    }
    for (int factor : {2, 4, 8}) {
        Oversampler<float> os;
        os.setFactor(factor);
        Adaa1<TanhSaturator> a;
        a.reset();
        refs.check("oversampler_x" + std::to_string(factor),
                   map(x, [&](float v) { return os.process(5.0f * v, [&](float u) { return a.process(u); }); }));
    }
    refs.check("adaa_tanh", adaaRun<TanhSaturator>(x));
    refs.check("adaa_asymmetric", adaaRun<AsymmetricTanhSaturator>(x));
    refs.check("adaa_hardclip", adaaRun<HardClipSaturator>(x));
    refs.check("adaa_pade", adaaRun<PadeSaturator>(x));
    {
        // a zero-delay feedback one-pole with a tanh in the loop, solved per sample
        float s = 0.0f, yPrev = 0.0f;
        const float G = 0.3f;
        std::vector<float> y;
        for (float v : x) {
            const float f0 = s + G * 3.0f * v;
            const float out = NewtonSolver<TanhSaturator, 4>::solve(f0, G, 1.0f, yPrev);
            const float out2 = FixedPointSolver<PadeSaturator, 6>::solve(f0, G, 1.0f, yPrev);
            s = 2.0f * out - s;
            yPrev = out;
            y.push_back(out);
            y.push_back(out2);
        }
        refs.check("solvers", y);
    }

    // oscillator helpers and sources
    refs.check("blep2_saw", blepSaw<2>());
    refs.check("blep4_saw", blepSaw<4>());
    refs.check("blep16_saw", blepSaw<16>());
    refs.check("blep32_saw", blepSaw<32>());
    {
        pt2::MinBlep<float> b;
        b.reset();
        std::vector<float> y;
        double phase = 0.0;
        const double inc = 987.6 / kFs;
        float level = 1.0f;
        for (int i = 0; i < 4096; ++i) {
            phase += inc;
            if (phase >= 1.0) {
                phase -= 1.0;
                b.add(static_cast<float>(phase / inc), level - (-level));
                level = -level;
            }
            y.push_back(b.run(level));
        }
        refs.check("pt2_minblep", y);
    }
    {
        WhiteNoise<float> w;
        PinkNoise<float> p;
        w.seed(Pcg32(8, 9));
        p.seed(Pcg32(8, 9));
        std::vector<float> y;
        for (int i = 0; i < 4096; ++i) {
            y.push_back(w.tick());
            y.push_back(p.tick());
        }
        refs.check("noise", y);
    }
    {
        OuDrift<double> d;
        d.seed(Pcg32::derive(3, 1, 4, 1));
        d.setup(25.0, 3.0, 64.0 / kFs);
        d.reset();
        std::vector<float> y;
        for (int i = 0; i < 4096; ++i)
            y.push_back(static_cast<float>(d.step()));
        refs.check("ou_drift", y);
    }

    // modulation sources, noise colors and the chorus
    {
        Adsr<float> env;
        env.prepare(kFs);
        env.setAttack(0.004);
        env.setDecay(0.02);
        env.setSustain(0.4f);
        env.setRelease(0.03);
        std::vector<float> y;
        for (int i = 0; i < 4096; ++i) {
            if (i == 0 || i == 2200)
                env.gateOn();
            if (i == 1200 || i == 3400)
                env.gateOff();
            env.tick();
            y.push_back(env.value());
            y.push_back(env.shaped(0.6f));
        }
        refs.check("adsr", y);
    }
    {
        std::vector<float> y;
        for (int w = 0; w < kWaveCount; ++w) {
            Lfo<float> lfo;
            lfo.prepare(kFs);
            lfo.setRate(37.0);
            lfo.setWave(static_cast<Wave>(w));
            lfo.seed(Pcg32(5 + w, 3));
            for (int i = 0; i < 1024; ++i)
                y.push_back(lfo.tick());
        }
        refs.check("lfo", y);
    }
    {
        Chorus<float> ch;
        ch.prepare(kFs);
        ch.setRate(0.8);
        ch.setDepthMs(5.0);
        ch.setMix(0.6f);
        std::vector<float> y;
        for (size_t i = 0; i < x.size(); ++i) {
            float l = x[i], r = x[x.size() - 1 - i];
            ch.process(l, r);
            y.push_back(l);
            y.push_back(r);
        }
        refs.check("chorus", y);
    }
    {
        BrownNoise<float> b;
        b.prepare(kFs);
        b.seed(Pcg32(12, 1));
        CrackleNoise<float> c;
        c.prepare(kFs);
        c.setRate(800.0);
        c.seed(Pcg32(13, 2));
        std::vector<float> y;
        for (int i = 0; i < 4096; ++i) {
            y.push_back(b.tick());
            y.push_back(c.tick());
        }
        refs.check("noise_colors", y);
    }
}

TEST_CASE("every block renders the same for any block size") {
    // Blocks keep their state per sample, so splitting changes nothing; checked on the stateful chain
    // oversampler + ADAA + SVF with block sizes 1, 13 and 4096.
    const auto x = spux_test::stimulus();
    auto render = [&](int block) {
        Oversampler<float> os;
        os.setFactor(4);
        Adaa1<TanhSaturator> a;
        a.reset();
        Svf<float> s;
        s.setCutoff(900.0, 2.0, kFs);
        std::vector<float> y(x.size());
        for (size_t i = 0; i < x.size(); i += static_cast<size_t>(block)) {
            const size_t end = std::min(x.size(), i + static_cast<size_t>(block));
            for (size_t j = i; j < end; ++j)
                y[j] = s.tick(os.process(3.0f * x[j], [&](float u) { return a.process(u); })).low;
        }
        return y;
    };
    const auto ref = render(4096);
    CHECK(render(1) == ref);
    CHECK(render(13) == ref);
}
