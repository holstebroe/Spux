#include <doctest.h>

#include <cmath>
#include <limits>

#include "spux/testing/RenderSafety.hpp"

using namespace spux::testing;

TEST_CASE("a quiet finite render is safe") {
    const std::vector<float> x = {0.f, 0.5f, -0.99f, 0.1f};
    const SafetyReport r = checkRenderSafety(x);
    CHECK(r.ok());
    CHECK(r.peak == doctest::Approx(0.99));
    CHECK(r.peakIndex == 2);
}

TEST_CASE("full scale is not below 1") {
    CHECK_FALSE(checkRenderSafety(std::vector<float>{0.f, 1.0f}).ok());
    CHECK(checkRenderSafety(std::vector<float>{0.f, 1.0f}, 1.5).ok()); // the limit is a parameter
    CHECK_FALSE(checkRenderSafety(std::vector<float>{-1.2f}).ok());
}

TEST_CASE("non-finite samples are found and counted") {
    std::vector<float> x(100, 0.1f);
    x[40] = std::nanf("");
    x[60] = -std::numeric_limits<float>::infinity();
    const SafetyReport r = checkRenderSafety(x);
    CHECK_FALSE(r.ok());
    CHECK(r.nonFinite == 2);
    CHECK(r.firstNonFinite == 40);
    CHECK(r.peak == doctest::Approx(0.1)); // the finite samples still give a peak
    CHECK(r.describe().find("non-finite") != std::string::npos);
}

TEST_CASE("a silent render fails when a minimum peak is required") {
    const std::vector<float> z(64, 0.f);
    CHECK(checkRenderSafety(z).ok());
    CHECK_FALSE(checkRenderSafety(z, 1.0, 0.01).ok());
    CHECK(checkRenderSafety(z, 1.0, 0.01).describe().find("silent") != std::string::npos);
}

TEST_CASE("stereo covers both channels") {
    std::vector<float> l(10, 0.2f), r(10, 0.3f);
    r[4] = 1.5f;
    const SafetyReport s = checkRenderSafety(l, r);
    CHECK_FALSE(s.ok());
    CHECK(s.peak == doctest::Approx(1.5));
    CHECK(s.peakIndex == 14);
    CHECK(s.samples == 20);
    r[4] = std::nanf("");
    CHECK(checkRenderSafety(l, r).firstNonFinite == 14);
}
