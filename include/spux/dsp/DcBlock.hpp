// DC blocker: one-pole high-pass y = x - x1 + R y1 with R = 1 - 2 pi fc / fs (fc = 8 Hz
// suits filter and drive stages). R is derived from the cutoff and the rate (R4), never fixed.
#pragma once

#include <concepts>

#include "spux/core/Math.hpp"

namespace spux::inline v1::dsp {

template <std::floating_point T = float> class DcBlock {
public:
    // fc well below fs (the law is the small-angle form of the pole).
    void setCutoff(double cutoffHz, double sampleRate) {
        R_ = static_cast<T>(1.0 - math::kTwoPi * cutoffHz / sampleRate);
    }
    void reset() { x1_ = y1_ = T(0); }

    T tick(T x) {
        const T y = x - x1_ + R_ * y1_;
        x1_ = x;
        y1_ = y;
        return y;
    }
    void process(T* data, int n) {
        for (int i = 0; i < n; ++i)
            data[i] = tick(data[i]);
    }

private:
    T R_ = T(0.995);
    T x1_ = T(0), y1_ = T(0);
};

} // namespace spux::inline v1::dsp
