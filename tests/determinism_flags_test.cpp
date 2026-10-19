// Checks that spux_target_flags delivered the reproducibility flags (R3): no fast-math, no
// contraction of a*b+c into a fused multiply-add.
#include <doctest.h>

#include <cfloat>

#ifdef __FAST_MATH__
#error "fast-math must never be enabled"
#endif

static_assert(FLT_EVAL_METHOD == 0, "floating-point expressions must be evaluated in their own type");

TEST_CASE("a*b+c is not fused") {
    // x*x = 1 + 2^-29 + 2^-60. Rounded to double the 2^-60 term is lost, so the unfused difference
    // is exactly zero; a fused multiply-add would keep it and return 2^-60.
    volatile double x = 1.0 + 0x1p-30;
    volatile double c = -(1.0 + 0x1p-29);
    const double r = x * x + c;
    CHECK(r == 0.0);
}
