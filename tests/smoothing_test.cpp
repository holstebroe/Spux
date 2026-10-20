// Control tick, tick ramps, smoothing policies and the denormal guard.
#include <doctest.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include "spux/core/ControlTick.hpp"
#include "spux/core/DenormalGuard.hpp"
#include "spux/core/Smoothing.hpp"

using namespace spux;

TEST_CASE("ControlTick splits blocks at the tick boundaries, independent of the block size") {
    for (int block : {1, 7, 64, 100, 333}) {
        ControlTick<64> tick;
        std::vector<int> ticksAt;
        int done = 0, samples = 0;
        while (done < 1000) {
            const int n = std::min(block, 1000 - done);
            tick.run(n, [&] { ticksAt.push_back(done + samples); }, [&](int, int count) { samples += count; });
            done += n;
            samples = 0;
        }
        REQUIRE(ticksAt.size() == 16);
        for (size_t i = 0; i < ticksAt.size(); ++i)
            CHECK(ticksAt[i] == static_cast<int>(64 * i));
    }
    ControlTick<16> t;
    t.advance(20);
    CHECK(t.position() == 4);
    CHECK(t.remaining() == 12);
}

TEST_CASE("TickRamp goes from start to end over one tick") {
    TickRamp<float, 64> r{1.0f, 3.0f};
    CHECK(r.at(0) == 1.0f);
    CHECK(r.at(32) == 2.0f);
    CHECK(r.at(64) == 3.0f);
    r.next(5.0f);
    CHECK(r.start == 3.0f);
    CHECK(r.end == 5.0f);
    r.next(7.0f, true);
    CHECK(r.start == 7.0f);
    CHECK(TickRamp<double, 8>::constant(2.0).at(5) == 2.0);
}

TEST_CASE("NoSmoothing and LinearRamp") {
    Smoothed<NoSmoothing> none;
    none.reset(1.0f);
    none.setTarget(2.0f);
    CHECK(none.next() == 2.0f);
    CHECK(none.settled());

    Smoothed<LinearRamp<4>> lin;
    lin.reset(0.0f);
    lin.setTarget(1.0f);
    CHECK_FALSE(lin.settled());
    float out[6];
    lin.process(out, 6);
    const float expected[6] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.0f};
    for (int i = 0; i < 6; ++i)
        CHECK(out[i] == expected[i]);
    CHECK(lin.settled());
    // a new target mid-ramp continues from where the ramp is
    lin.reset(0.0f);
    lin.setTarget(1.0f);
    lin.skip(2); // 0, 0.25; the next sample would be 0.5
    lin.setTarget(0.0f);
    CHECK(lin.next() == 0.5f);
}

TEST_CASE("OnePole reaches 63 % after Tau and lands exactly") {
    Smoothed<OnePole<0.01>, double> s;
    s.prepare(48000.0);
    s.reset(0.0);
    s.setTarget(1.0);
    s.skip(480);
    CHECK(s.current() == doctest::Approx(1.0 - std::exp(-1.0)).epsilon(1e-3));
    Smoothed<OnePole<0.01>, float> f;
    f.prepare(48000.0);
    f.reset(0.0f);
    f.setTarget(1.0f);
    int n = 0;
    while (!f.settled() && n < 100000) {
        f.next();
        ++n;
    }
    CHECK(f.current() == 1.0f);
    CHECK(n < 48000);
    CHECK(onePoleCoeff(0.0, 48000.0) == 1.0);
    CHECK(onePoleCoeff<float>(1.0, 1.0) == doctest::Approx(1.0 - std::exp(-1.0)));
}

TEST_CASE("Slew moves at its rate") {
    Smoothed<Slew<100.0>> s; // 100 units per second
    s.prepare(1000.0);       // 0.1 per sample
    s.reset(0.0f);
    s.setTarget(1.0f);
    s.skip(5);
    CHECK(s.current() == doctest::Approx(0.5f));
    s.skip(10);
    CHECK(s.current() == 1.0f);
    CHECK(s.settled());
    s.setTarget(-1.0f);
    CHECK(s.next() == doctest::Approx(0.9f));
}

TEST_CASE("smoothing is independent of the block size") {
    auto render = [](int block) {
        Smoothed<OnePole<0.002>> s;
        s.prepare(44100.0);
        s.reset(0.0f);
        std::vector<float> out(1000);
        for (int i = 0; i < 1000; i += block) {
            if (i % 64 == 0)
                s.setTarget(static_cast<float>((i / 64) % 3));
            s.process(out.data() + i, std::min(block, 1000 - i));
        }
        return out;
    };
    // targets change only at multiples of 64, so blocks of 64 and 32 must agree bit for bit
    CHECK(render(64) == render(32));
    CHECK(render(64) == render(1));
}

TEST_CASE("DenormalGuard flushes denormals and restores the environment") {
#if defined(SPUX_FP_GUARD_SSE) || defined(SPUX_FP_GUARD_ARM64)
    volatile float tiny = std::numeric_limits<float>::min();
    volatile float half = 0.5f;
    {
        DenormalGuard guard;
        const float r = tiny * half; // a denormal without the guard
        CHECK(r == 0.0f);
    }
    const float r = tiny * half;
    CHECK(r != 0.0f);
#else
    DenormalGuard guard;
    CHECK(true);
#endif
}
