// PCG32: the reference sequence of the PCG paper's demo, the draw helpers and seed derivation.
#include <doctest.h>

#include <cmath>
#include <cstdint>

#include "spux/core/Pcg32.hpp"

using spux::Pcg32;

TEST_CASE("matches the PCG reference sequence (pcg32-demo, seed 42, stream 54)") {
    Pcg32 rng(42u, 54u);
    const std::uint32_t expected[] = {0xa15c02b7u, 0x7b47f409u, 0xba1d3330u, 0x83d2f293u, 0xbfa4784bu, 0xcbed606eu};
    for (std::uint32_t e : expected)
        CHECK(rng.next() == e);
    CHECK(Pcg32(42u) == Pcg32(42u, 54u)); // stream 54 is the default
}

TEST_CASE("the default state is the reference initializer, and constexpr") {
    constexpr auto first = [] {
        Pcg32 r;
        return r.next();
    }();
    Pcg32 r;
    CHECK(r.next() == first);
    static_assert(Pcg32(1, 2) == Pcg32(1, 2));
    static_assert(!(Pcg32(1, 2) == Pcg32(1, 3)));
}

TEST_CASE("draws stay in range and have the expected moments") {
    Pcg32 rng(7, 1);
    double sum = 0, sum2 = 0, gsum = 0, gsum2 = 0;
    constexpr int n = 200000;
    for (int i = 0; i < n; ++i) {
        const float f = rng.nextFloat();
        CHECK_UNARY(f >= 0.0f);
        CHECK_UNARY(f < 1.0f);
        const double d = rng.nextDouble();
        REQUIRE(d >= 0.0);
        REQUIRE(d < 1.0);
        const float b = rng.nextBipolar();
        REQUIRE(b >= -1.0f);
        REQUIRE(b < 1.0f);
        sum += d;
        sum2 += d * d;
        const double g = rng.nextGaussian();
        gsum += g;
        gsum2 += g * g;
    }
    CHECK(sum / n == doctest::Approx(0.5).epsilon(0.01));
    CHECK(sum2 / n - (sum / n) * (sum / n) == doctest::Approx(1.0 / 12.0).epsilon(0.02));
    CHECK(std::fabs(gsum / n) < 0.01);
    CHECK(gsum2 / n == doctest::Approx(1.0).epsilon(0.02));
}

TEST_CASE("nextBelow is uniform and in range") {
    Pcg32 rng(3, 9);
    int count[7] = {};
    for (int i = 0; i < 70000; ++i) {
        const auto v = rng.nextBelow(7);
        REQUIRE(v < 7u);
        ++count[v];
    }
    for (int c : count)
        CHECK(c == doctest::Approx(10000).epsilon(0.05));
}

TEST_CASE("derive depends on every input and is repeatable") {
    const auto a = Pcg32::derive(1, 2, 3, 4);
    CHECK(a == Pcg32::derive(1, 2, 3, 4));
    CHECK_FALSE(a == Pcg32::derive(9, 2, 3, 4));
    CHECK_FALSE(a == Pcg32::derive(1, 9, 3, 4));
    CHECK_FALSE(a == Pcg32::derive(1, 2, 9, 4));
    CHECK_FALSE(a == Pcg32::derive(1, 2, 3, 9));
}
