// Floating-point environment for audio processing (R6): hold one DenormalGuard across process(). It sets
// round to nearest, masks all exceptions and flushes denormals to zero, and restores the host's state on
// destruction. So decaying tails never take the slow denormal path, and neither a host's rounding mode
// nor its exception masks reach the DSP (R3).
//
// x86 and x64: MXCSR with FTZ and DAZ. ARM64: FPCR with FZ. Elsewhere it does nothing.
// From Lumeoid `dsp/simd.hpp` (Plumeflow's guard only added FTZ and DAZ and kept the other bits; the
// result is the same under any host that leaves rounding and masks at their defaults).
#pragma once

#include <cstdint>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define SPUX_FP_GUARD_SSE 1
#include <xmmintrin.h>
#elif defined(_M_ARM64)
#define SPUX_FP_GUARD_ARM64 1
#include <intrin.h>
#elif defined(__aarch64__)
#define SPUX_FP_GUARD_ARM64 1
#endif

namespace spux::inline v1 {

class DenormalGuard {
public:
#if defined(SPUX_FP_GUARD_SSE)
    DenormalGuard() : saved_(_mm_getcsr()) { _mm_setcsr(kMode); }
    ~DenormalGuard() { _mm_setcsr(saved_); }

private:
    // Flush-to-zero (bit 15), the six exception masks (bits 7-12), denormals-are-zero (bit 6); rounding
    // bits 13-14 clear (to nearest) and status flags clear.
    static constexpr unsigned kMode = 0x9FC0u;
    unsigned saved_;
#elif defined(SPUX_FP_GUARD_ARM64)
    DenormalGuard() : saved_(readFpcr()) { writeFpcr(kMode); }
    ~DenormalGuard() { writeFpcr(saved_); }

private:
    // FZ (bit 24) set; rounding bits 22-23 clear (to nearest), trap enables clear, default NaN off.
    static constexpr std::uint64_t kMode = 1ull << 24;
#if defined(_M_ARM64)
    static constexpr int kFpcr = 0x5A20; // ARM64_SYSREG(3, 3, 4, 4, 0)
    static std::uint64_t readFpcr() { return static_cast<std::uint64_t>(_ReadStatusReg(kFpcr)); }
    static void writeFpcr(std::uint64_t v) { _WriteStatusReg(kFpcr, static_cast<__int64>(v)); }
#else
    static std::uint64_t readFpcr() {
        std::uint64_t v;
        __asm__ __volatile__("mrs %0, fpcr" : "=r"(v));
        return v;
    }
    static void writeFpcr(std::uint64_t v) { __asm__ __volatile__("msr fpcr, %0" : : "r"(v)); }
#endif
    std::uint64_t saved_;
#else
    DenormalGuard() {}
    ~DenormalGuard() {}
#endif

public:
    DenormalGuard(const DenormalGuard&) = delete;
    DenormalGuard& operator=(const DenormalGuard&) = delete;
};

} // namespace spux::inline v1
