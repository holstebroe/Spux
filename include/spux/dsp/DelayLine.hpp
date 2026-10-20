// Circular delay line with integer, linear and 4-point cubic Hermite reads.
//
// Read before write: delay 1 is the sample of the last write(). Integer and linear reads need
// delay >= 1, Hermite reads delay >= 2, all at most maxDelay(). The capacity is a power of two,
// allocated in prepare() only (not real time); reads and writes are real-time safe.
#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <vector>

namespace spux::inline v1::dsp {

template <std::floating_point T = float> class DelayLine {
public:
    // Room for reads up to `maxDelaySamples`. Allocates.
    void prepare(std::size_t maxDelaySamples) {
        std::size_t n = 16;
        while (n < maxDelaySamples + 8)
            n <<= 1;
        buf_.assign(n, T(0));
        mask_ = n - 1;
        pos_ = 0;
    }
    void clear() {
        std::fill(buf_.begin(), buf_.end(), T(0));
        pos_ = 0;
    }
    [[nodiscard]] std::size_t maxDelay() const { return buf_.empty() ? 0 : buf_.size() - 3; }

    void write(T v) {
        buf_[pos_] = v;
        pos_ = (pos_ + 1) & mask_;
    }

    [[nodiscard]] T readInt(std::size_t delay) const { return at(delay); }

    [[nodiscard]] T readLinear(double delay) const {
        const auto whole = static_cast<std::size_t>(delay);
        const T t = static_cast<T>(delay - static_cast<double>(whole));
        const T a = at(whole);
        return a + (at(whole + 1) - a) * t;
    }

    [[nodiscard]] T readHermite(double delay) const {
        const auto whole = static_cast<std::size_t>(delay);
        const T t = static_cast<T>(delay - static_cast<double>(whole));
        const T ym1 = at(whole - 1);
        const T y0 = at(whole);
        const T y1 = at(whole + 1);
        const T y2 = at(whole + 2);
        const T c1 = T(0.5) * (y1 - ym1);
        const T c2 = ym1 - T(2.5) * y0 + T(2) * y1 - T(0.5) * y2;
        const T c3 = T(0.5) * (y2 - ym1) + T(1.5) * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }

private:
    [[nodiscard]] T at(std::size_t back) const { return buf_[(pos_ - back) & mask_]; }

    std::vector<T> buf_;
    std::size_t mask_ = 0;
    std::size_t pos_ = 0;
};

} // namespace spux::inline v1::dsp
