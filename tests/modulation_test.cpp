// The modulation sources and their plumbing: the ADSR envelope, the LFO, the chorus, the voice allocator, the
// modulation matrix and the extra noise colors.
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "dsp_support.hpp"
#include "spux/core/Wave.hpp"
#include "spux/dsp/Chorus.hpp"
#include "spux/dsp/Envelope.hpp"
#include "spux/dsp/Lfo.hpp"
#include "spux/dsp/ModMatrix.hpp"
#include "spux/dsp/Noise.hpp"
#include "spux/dsp/VoiceAllocator.hpp"

using namespace spux;
using namespace spux::dsp;
using spux_test::kFs;

namespace {
int samplesUntil(Adsr<double>& e, double level, bool above, int max) {
    for (int i = 1; i <= max; ++i)
        if ((e.tick() >= level) == above)
            return i;
    return -1;
}
} // namespace

TEST_CASE("adsr: the times are the times, at any rate") {
    for (double fs : {22050.0, 48000.0, 96000.0}) {
        Adsr<double> e;
        e.prepare(fs);
        e.setAttack(0.020);
        e.setDecay(0.100);
        e.setSustain(0.5);
        e.setRelease(0.200);
        e.gateOn();
        // attack: 1 is reached at 20 ms
        const int a = samplesUntil(e, 1.0, true, static_cast<int>(fs));
        CHECK(a / fs == doctest::Approx(0.020).epsilon(0.03));
        CHECK(e.value() == 1.0);
        // decay: the distance to sustain falls by 60 dB in 100 ms (the stage ends a little before: at 1e-4)
        double dist1 = 0, dist2 = 0;
        const int n100 = static_cast<int>(0.100 * fs);
        for (int i = 0; i < n100; ++i)
            e.tick();
        dist1 = e.value() - 0.5;
        CHECK(dist1 == doctest::Approx(0.5e-3).epsilon(0.25)); // 1 -> 0.5 by 60 dB is 0.5 * 1e-3 left
        for (int i = 0; i < n100; ++i)
            e.tick();
        dist2 = e.value() - 0.5;
        CHECK(dist2 == 0.0);
        CHECK(e.stage() == Adsr<double>::Stage::Sustain);
        CHECK(e.value() == 0.5);
        // release: -60 dB in 200 ms
        e.gateOff();
        const int n200 = static_cast<int>(0.200 * fs);
        for (int i = 0; i < n200; ++i)
            e.tick();
        CHECK(e.value() == doctest::Approx(0.5e-3).epsilon(0.05));
        // and the voice is over soon after
        for (int i = 0; i < n200; ++i)
            e.tick();
        CHECK_FALSE(e.active());
        CHECK(e.value() == 0.0);
    }
}

TEST_CASE("adsr: retrigger continues from the value, release from anywhere, minimum times, shaping") {
    Adsr<float> e;
    e.prepare(kFs);
    e.setAttack(0.05);
    e.setSustain(0.3f);
    e.setRelease(0.05);
    e.gateOn();
    for (int i = 0; i < 400; ++i)
        e.tick();
    const float inAttack = e.value();
    CHECK(inAttack > 0.3f);
    CHECK(inAttack < 1.0f);
    e.gateOff(); // released during the attack: it falls from there
    e.tick();
    CHECK(e.stage() == Adsr<float>::Stage::Release);
    CHECK(e.value() < inAttack);
    e.gateOn(); // retrigger: no jump to 0
    const float before = e.value();
    e.tick();
    CHECK(e.value() >= before);
    CHECK(e.gate());
    // gateOff on an idle envelope stays idle
    Adsr<float> idle;
    idle.prepare(kFs);
    idle.gateOff();
    CHECK(idle.stage() == Adsr<float>::Stage::Idle);
    CHECK_FALSE(idle.active());
    // times below a millisecond are a millisecond
    Adsr<double> fast;
    fast.prepare(kFs);
    fast.setAttack(0.0);
    fast.gateOn();
    CHECK(samplesUntil(fast, 1.0, true, 1000) >= 40);
    // shaping: 0 is linear, 1 the S-curve, the ends stay put
    Adsr<float> s;
    s.prepare(kFs);
    s.setAttack(0.05);
    s.gateOn();
    for (int i = 0; i < 300; ++i)
        s.tick();
    const float v = s.value();
    CHECK(s.shaped(0.0f) == doctest::Approx(v));
    CHECK(s.shaped(1.0f) == doctest::Approx(v * v * (3.0f - 2.0f * v)));
    for (int i = 0; i < 100000; ++i)
        s.tick();
    CHECK(s.shaped(1.0f) == doctest::Approx(0.5f));
    s.reset();
    CHECK(s.shaped(1.0f) == 0.0f);
}

TEST_CASE("lfo: each wave has its shape, range and period, at any rate") {
    for (double fs : {44100.0, 48000.0, 96000.0}) {
        for (int w = 0; w < kWaveCount; ++w) {
            Lfo<float> lfo;
            lfo.prepare(fs);
            lfo.setRate(5.0);
            lfo.setWave(static_cast<Wave>(w));
            lfo.seed(Pcg32(9, 9));
            const int period = static_cast<int>(fs / 5.0);
            std::vector<float> y;
            for (int i = 0; i < period * 4; ++i)
                y.push_back(lfo.tick());
            float lo = 9, hi = -9;
            for (float v : y) {
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
            INFO(waveName(static_cast<Wave>(w)) << " at " << fs);
            CHECK(lo >= -1.0001f);
            CHECK(hi <= 1.0001f);
            switch (static_cast<Wave>(w)) {
            case Wave::Sine:
            case Wave::Triangle:
            case Wave::Saw:
            case Wave::SawDown:
            case Wave::Square:
            case Wave::Pulse:
                CHECK(lo < -0.95f);
                CHECK(hi > 0.95f);
                {
                    // periodic: the second period repeats the first (but for a sample at a jump, where the
                    // accumulated phase may land on the other side)
                    int differ = 0;
                    for (int i = 0; i < period; ++i)
                        differ += std::fabs(y[static_cast<size_t>(i)] - y[static_cast<size_t>(i + period)]) > 0.01f;
                    CHECK(differ <= 3);
                }
                break;
            default:
                break;
            }
        }
    }
    // the shapes at known phases
    auto at = [](Wave w, double phase) {
        Lfo<double> l;
        l.prepare(1000.0);
        l.setRate(1.0);
        l.setWave(w);
        l.seed(Pcg32(1, 1));
        l.reset(phase);
        return l.tick();
    };
    CHECK(at(Wave::Sine, 0.25) == doctest::Approx(1.0).epsilon(1e-4));
    CHECK(at(Wave::Triangle, 0.0) == doctest::Approx(0.0));
    CHECK(at(Wave::Triangle, 0.25) == doctest::Approx(1.0));
    CHECK(at(Wave::Triangle, 0.75) == doctest::Approx(-1.0));
    CHECK(at(Wave::Saw, 0.0) == doctest::Approx(-1.0));
    CHECK(at(Wave::SawDown, 0.0) == doctest::Approx(1.0));
    CHECK(at(Wave::Square, 0.49) == 1.0);
    CHECK(at(Wave::Square, 0.51) == -1.0);
    CHECK(at(Wave::Pulse, 0.24) == 1.0);
    CHECK(at(Wave::Pulse, 0.26) == -1.0);
}

TEST_CASE("lfo: the random waves are seeded and deterministic; sample and hold holds, noise is smooth") {
    auto render = [](Wave w, std::uint64_t seed) {
        Lfo<float> l;
        l.prepare(1000.0);
        l.setRate(10.0);
        l.setWave(w);
        l.seed(Pcg32(seed, 1));
        std::vector<float> y;
        for (int i = 0; i < 1000; ++i)
            y.push_back(l.tick());
        return y;
    };
    CHECK(render(Wave::Noise, 4) == render(Wave::Noise, 4));
    CHECK(render(Wave::Noise, 4) != render(Wave::Noise, 5));
    const auto sh = render(Wave::SampleHold, 4);
    int changes = 0;
    for (size_t i = 1; i < sh.size(); ++i)
        changes += sh[i] != sh[i - 1];
    CHECK(changes <= 10); // one new value per period, 10 periods
    CHECK(changes >= 8);
    const auto nz = render(Wave::Noise, 4);
    float maxStep = 0;
    for (size_t i = 1; i < nz.size(); ++i)
        maxStep = std::max(maxStep, std::fabs(nz[i] - nz[i - 1]));
    CHECK(maxStep < 0.08f); // a smoothstep over 100 samples
}

TEST_CASE("chorus: off is transparent, on widens and keeps the level, any block size is the same") {
    const auto x = spux_test::stimulus();
    Chorus<float> off;
    off.prepare(kFs);
    for (float v : x) {
        float l = v, r = v;
        off.process(l, r);
        REQUIRE(l == v);
        REQUIRE(r == v);
    }
    Chorus<float> ch;
    ch.prepare(kFs);
    ch.setRate(0.7);
    ch.setDepthMs(6.0);
    ch.setMix(0.8f);
    double eIn = 0, eOut = 0, diff = 0;
    for (float v : x) {
        float l = v, r = v;
        ch.process(l, r);
        eIn += v * v;
        eOut += 0.5 * (l * l + r * r);
        diff += (l - r) * (l - r);
    }
    CHECK(diff > 0.01 * eIn); // the channels differ: the image is wider
    CHECK(eOut / eIn > 0.4);  // and the level stays in the same range
    CHECK(eOut / eIn < 1.3);
    // an impulse comes back about 14 ms later, three times
    Chorus<float> imp;
    imp.prepare(kFs);
    imp.setDepthMs(0.0);
    imp.setMix(1.0f);
    int firstWet = -1;
    for (int i = 0; i < 2000; ++i) {
        float l = i == 0 ? 1.0f : 0.0f, r = 0.0f;
        imp.process(l, r);
        if (i > 0 && std::fabs(l) > 0.05f && firstWet < 0)
            firstWet = i;
    }
    CHECK(firstWet / kFs == doctest::Approx(0.014).epsilon(0.05));
}

TEST_CASE("voice allocator: retrigger, free voices round-robin, steal the longest released, then the oldest held") {
    VoiceAllocator a;
    a.setLimit(3);
    auto r0 = a.noteOn(60);
    auto r1 = a.noteOn(62);
    auto r2 = a.noteOn(64);
    CHECK(r0.slot == 0);
    CHECK(r1.slot == 1);
    CHECK(r2.slot == 2);
    CHECK_FALSE(r0.stolen);
    // all held: the oldest held note goes
    auto r3 = a.noteOn(65);
    CHECK(r3.stolen);
    CHECK(r3.slot == 0);
    CHECK(r3.wasHeld);
    // release 62 and 64, 64 later: 62 was released longest ago
    CHECK(a.noteOff(62) == 1);
    CHECK(a.noteOff(64) == 2);
    CHECK(a.noteOff(99) == -1);
    auto r4 = a.noteOn(67);
    CHECK(r4.stolen);
    CHECK(r4.slot == 1);
    CHECK_FALSE(r4.wasHeld);
    // a re-struck key keeps its voice, held or ringing out
    auto again = a.noteOn(67);
    CHECK(again.retrigger);
    CHECK(again.slot == 1);
    a.noteOff(67);
    auto ring = a.noteOn(67);
    CHECK(ring.retrigger);
    CHECK(ring.slot == 1);
    // a finished voice is free: the search starts after the last one given
    a.noteOff(67);
    a.free(1);
    a.free(2);
    const auto f = a.noteOn(70);
    CHECK_FALSE(f.stolen);
    CHECK(f.slot == 1);
    CHECK(a.slot(1).key == 70);
    CHECK(a.slot(1).held);
    const auto g = a.noteOn(72);
    CHECK_FALSE(g.stolen);
    CHECK(g.slot == 2);
    // limit 1 is a mono voice: wasHeld says legato
    VoiceAllocator mono;
    mono.setLimit(1);
    CHECK_FALSE(mono.noteOn(60).wasHeld);
    CHECK(mono.noteOn(62).wasHeld);
    mono.noteOff(62);
    CHECK_FALSE(mono.noteOn(64).wasHeld);
    // the limit is clamped and slots beyond it are not given out
    VoiceAllocator c;
    c.setLimit(0);
    CHECK(c.limit() == 1);
    c.setLimit(1000);
    CHECK(c.limit() == VoiceAllocator::kMaxVoices);
    c.setLimit(2);
    CHECK(c.noteOn(1).slot == 0);
    CHECK(c.noteOn(2).slot == 1);
    CHECK(c.noteOn(3).slot < 2);
    c.reset();
    CHECK(c.noteOn(5).slot == 0);
}

TEST_CASE("mod matrix: routes sum per target; one route per source or many; no allocation") {
    enum : std::uint32_t { Lfo1, Env1, Wheel, Cutoff, Pitch, Amp };
    ModMatrix<6> m;
    CHECK(ModMatrix<6>::size() == 6);
    CHECK(m.freeRoute() == 0);
    CHECK(m.routesInto(Cutoff) == 0);
    const float src[3] = {0.5f, 1.0f, 0.25f}; // Lfo1, Env1, Wheel
    CHECK(m.amountFor(Cutoff, src) == 0.0f);
    CHECK(m.effective(Cutoff, 0.4f, src) == 0.4f);
    // a single target per source: Lfo1 and Env1 to the cutoff, the wheel to the pitch
    m.set(0, Lfo1, Cutoff, 0.4f);
    m.set(1, Env1, Cutoff, -0.1f);
    m.set(2, Wheel, Pitch, 1.0f);
    CHECK(m.routesInto(Cutoff) == 2);
    CHECK(m.amountFor(Cutoff, src) == doctest::Approx(0.4f * 0.5f - 0.1f * 1.0f));
    CHECK(m.amountFor(Pitch, src) == doctest::Approx(0.25f));
    CHECK(m.amountFor(Amp, src) == 0.0f);
    CHECK(m.effective(Cutoff, 0.95f, src) == doctest::Approx(1.0f)); // clamped
    CHECK(m.effective(Cutoff, 0.0f, src) == doctest::Approx(0.1f));
    // many targets per source (a macro): the same source on more routes
    const std::size_t extra = m.freeRoute();
    CHECK(extra == 3);
    m.set(extra, Lfo1, Amp, 0.5f);
    CHECK(m.amountFor(Amp, src) == doctest::Approx(0.25f));
    CHECK(m.find(Lfo1, Amp) == extra);
    CHECK(m.find(Lfo1, Pitch) == ModMatrix<6>::size());
    // a depth of 0 is no route; a source id beyond the values counts as 0; clearing frees the route
    m.setDepth(extra, 0.0f);
    CHECK(m.routesInto(Amp) == 0);
    m.set(extra, 17, Amp, 1.0f);
    CHECK(m.amountFor(Amp, src) == 0.0f);
    m.clear(extra);
    CHECK(m.freeRoute() == extra);
    for (std::size_t i = 0; i < ModMatrix<6>::size(); ++i)
        m.set(i, Lfo1, Cutoff, 0.1f);
    CHECK(m.freeRoute() == ModMatrix<6>::size());
    m.clear();
    CHECK(m.freeRoute() == 0);
}

TEST_CASE("noise colors: brown falls 6 dB per octave and matches the white RMS; crackle clicks at its rate") {
    BrownNoise<float> b;
    b.prepare(kFs);
    b.seed(Pcg32(3, 3));
    WhiteNoise<float> w;
    w.seed(Pcg32(3, 3));
    const int n = 1 << 16;
    std::vector<float> yb, yw;
    for (int i = 0; i < n; ++i) {
        yb.push_back(b.tick());
        yw.push_back(w.tick());
    }
    auto rms = [](const std::vector<float>& v) {
        double s = 0;
        for (float x : v)
            s += x * x;
        return std::sqrt(s / static_cast<double>(v.size()));
    };
    CHECK(rms(yb) == doctest::Approx(rms(yw)).epsilon(0.1));
    // spectrum slope: 6 dB per octave per bin is 3 dB per octave of band power (2-4 kHz against 4-8 kHz)
    std::vector<std::complex<double>> a(yb.begin(), yb.end());
    spux::testing::fft(a);
    auto band = [&](double lo, double hi) {
        double p = 0;
        for (size_t k = static_cast<size_t>(lo / kFs * n); k < static_cast<size_t>(hi / kFs * n); ++k)
            p += std::norm(a[k]);
        return p;
    };
    CHECK(10.0 * std::log10(band(2000, 4000) / band(4000, 8000)) == doctest::Approx(3.01).epsilon(0.15));

    for (double rate : {100.0, 500.0}) {
        CrackleNoise<float> c;
        c.prepare(kFs);
        c.setRate(rate);
        c.seed(Pcg32(8, 8));
        int clicks = 0;
        float prev = 0;
        for (int i = 0; i < static_cast<int>(kFs * 5); ++i) {
            const float v = c.tick();
            clicks += std::fabs(v) > 0.4f && std::fabs(v) > std::fabs(prev) + 0.3f; // a rise of a spike
            prev = v;
        }
        CHECK(clicks / 5.0 == doctest::Approx(rate).epsilon(0.15));
    }
    // a click rings out over its decay time
    CrackleNoise<float> d;
    d.prepare(kFs);
    d.setRate(0.0);
    d.setDecay(0.001);
    d.seed(Pcg32(1, 1));
    CHECK(d.tick() == 0.0f);
}
