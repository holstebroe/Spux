// The control tick (R5, decision S9): control-rate work (modulation, coefficient updates, parameter
// targets) happens every N samples, counted from the start of the render, never from the start of a
// host block. That keeps the output identical for any block size (R3). N is a template parameter,
// 64 by default (Lumeoid and Plumeflow); a plugin may use none.
#pragma once

#include <concepts>

namespace spux::inline v1 {

template <int N = 64>
    requires(N > 0)
class ControlTick {
public:
    static constexpr int kSize = N;

    constexpr void reset() { pos_ = 0; }
    // Position of the next sample inside the tick, 0 at a boundary.
    [[nodiscard]] constexpr int position() const { return pos_; }
    [[nodiscard]] constexpr bool atBoundary() const { return pos_ == 0; }
    // Samples until the next boundary (N at a boundary).
    [[nodiscard]] constexpr int remaining() const { return N - pos_; }
    constexpr void advance(int n) { pos_ = (pos_ + n) % N; }

    // Splits a block of n samples at the tick boundaries: onTick() runs at each boundary, then
    // run(offset, count) for the samples up to the next boundary or the end of the block.
    template <class OnTick, class Run> constexpr void run(int n, OnTick&& onTick, Run&& run) {
        int offset = 0;
        while (offset < n) {
            if (pos_ == 0)
                onTick();
            const int rest = n - offset;
            const int count = rest < N - pos_ ? rest : N - pos_;
            run(offset, count);
            advance(count);
            offset += count;
        }
    }

private:
    int pos_ = 0;
};

// A value that moves linearly across one tick (Lumeoid `Ramp`, `DoubleRamp`): `at(pos)` is the value at
// sample `pos` of the tick, from `start` at 0 to `end` at N (the first sample of the next tick). It is
// evaluated from the position, so it does not depend on how the host splits blocks. For T = float and
// N = 64 it is bit-identical to Lumeoid's Ramp.
template <std::floating_point T = float, int N = 64>
    requires(N > 0)
struct TickRamp {
    T start = T(0);
    T end = T(0);

    [[nodiscard]] constexpr T at(int tickPos) const {
        return start + (end - start) * (static_cast<T>(tickPos) * (T(1) / T(N)));
    }
    [[nodiscard]] static constexpr TickRamp constant(T v) { return {v, v}; }
    // Next tick: the ramp starts where the last one ended (or at `v` when `snap`) and ends at `v`.
    constexpr void next(T v, bool snap = false) {
        start = snap ? v : end;
        end = v;
    }
};

} // namespace spux::inline v1
