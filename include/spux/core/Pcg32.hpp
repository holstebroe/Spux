// PCG32 (O'Neill, XSH-RR 64/32): the one source of randomness in spux and the plugins (R3). Merged from
// Lumeoid `core/pcg32.hpp` and Plumeflow `dsp/Pcg32.hpp`, which produce the same sequence for the same
// seed and stream.
//
// Differences a plugin meets when it switches:
//   - Lumeoid: `reseed(seed, stream)` is `seed(seed, stream)`; everything else is the same.
//   - Plumeflow: the default constructor gives the PCG reference state (Lumeoid's), not seed(0, 1); a
//     generator that is used before it is seeded therefore starts elsewhere. The two-argument
//     constructor, `seed`, `derive` and all draws are unchanged.
#pragma once

#include <cstdint>
#include <initializer_list>

namespace spux::inline v1 {

class Pcg32 {
public:
    // The reference state of the PCG paper (PCG32_INITIALIZER).
    constexpr Pcg32() = default;
    constexpr Pcg32(std::uint64_t seedValue, std::uint64_t stream = 54u) { seed(seedValue, stream); }

    constexpr void seed(std::uint64_t seedValue, std::uint64_t stream = 54u) {
        state_ = 0u;
        inc_ = (stream << 1u) | 1u;
        next();
        state_ += seedValue;
        next();
    }

    constexpr std::uint32_t next() {
        const std::uint64_t old = state_;
        state_ = old * 6364136223846793005ull + inc_;
        const auto xorshifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
        const auto rot = static_cast<std::uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
    }

    // [0, 1) with 24 bits.
    constexpr float nextFloat() { return static_cast<float>(next() >> 8u) * (1.0f / 16777216.0f); }
    // [0, 1) with 32 bits.
    constexpr double nextDouble() { return static_cast<double>(next()) * (1.0 / 4294967296.0); }
    // [-1, 1).
    constexpr float nextBipolar() { return nextFloat() * 2.0f - 1.0f; }
    // Approximately Gaussian, zero mean, unit variance: the sum of six uniforms (no libm).
    constexpr double nextGaussian() {
        double s = 0;
        for (int i = 0; i < 6; ++i)
            s += nextDouble();
        return (s - 3.0) * 1.4142135623730951;
    }
    // Uniform integer in [0, bound), bound > 0, without modulo bias (rejection, as in the PCG library).
    constexpr std::uint32_t nextBelow(std::uint32_t bound) {
        const std::uint32_t threshold = (0u - bound) % bound;
        for (;;) {
            const std::uint32_t r = next();
            if (r >= threshold)
                return r % bound;
        }
    }

    // A generator derived from a patch seed, a voice slot and a note counter (never time or addresses),
    // so a render is the same for the same preset, seed and notes (Plumeflow spec 03).
    static constexpr Pcg32 derive(std::uint32_t patchSeed, std::uint32_t voiceSlot, std::uint32_t noteCounter,
                                  std::uint32_t salt = 0) {
        std::uint64_t h = 0x9E3779B97F4A7C15ull;
        for (std::uint32_t v : {patchSeed, voiceSlot, noteCounter, salt}) {
            h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
            h *= 0xBF58476D1CE4E5B9ull;
            h ^= h >> 31;
        }
        return Pcg32(h, voiceSlot * 2u + salt + 1u);
    }

    friend constexpr bool operator==(const Pcg32&, const Pcg32&) = default;

private:
    std::uint64_t state_ = 0x853c49e6748fea9bull;
    std::uint64_t inc_ = 0xda3e39cb94b95bdbull;
};

} // namespace spux::inline v1
