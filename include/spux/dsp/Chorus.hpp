// Three-voice stereo chorus. Each channel reads its own delay line three times, the voices a third of a period
// apart on a triangle LFO around a centre delay of 14 ms; the right channel runs half a period behind the left, so
// the wet signal widens the image. The output keeps the dry signal (1 - mix/2) and adds the wet at 0.7 mix.
//
//   prepare(fs)          allocates a 40 ms line per channel (not real time)
//   setRate(hz)          LFO rate, 0.01 to 10 Hz
//   setDepthMs(ms)       swing around the centre delay, 0 to 12 ms
//   setMix(mix)          0 (off: the signal passes untouched and costs nothing) to 1
#pragma once

#include <algorithm>
#include <concepts>

#include "spux/core/Math.hpp"
#include "spux/dsp/DelayLine.hpp"

namespace spux::inline v1::dsp {

template <std::floating_point T = float> class Chorus {
public:
    static constexpr int kVoices = 3;
    static constexpr double kCentreMs = 14.0, kMaxDepthMs = 12.0, kLineMs = 40.0;

    void prepare(double sampleRate) {
        fs_ = sampleRate;
        left_.prepare(static_cast<std::size_t>(sampleRate * kLineMs * 0.001) + 4);
        right_.prepare(static_cast<std::size_t>(sampleRate * kLineMs * 0.001) + 4);
        phase_ = 0.0;
    }
    void reset() {
        left_.clear();
        right_.clear();
        phase_ = 0.0;
    }
    void setRate(double hz) { rate_ = std::clamp(hz, 0.01, 10.0); }
    void setDepthMs(double ms) { depth_ = std::clamp(ms, 0.0, kMaxDepthMs); }
    void setMix(T mix) { mix_ = std::clamp(mix, T(0), T(1)); }

    void process(T& l, T& r) {
        left_.write(l);
        right_.write(r);
        phase_ += rate_ / fs_;
        phase_ -= math::floorD(phase_);
        if (mix_ <= T(0.0001))
            return;
        T wetL = T(0), wetR = T(0);
        for (int k = 0; k < kVoices; ++k) {
            const double pl = phase_ + static_cast<double>(k) / kVoices;
            const double pr = pl + 0.5;
            wetL += left_.readLinear((kCentreMs + depth_ * tri(pl)) * 0.001 * fs_);
            wetR += right_.readLinear((kCentreMs + depth_ * tri(pr)) * 0.001 * fs_);
        }
        wetL *= T(1) / T(kVoices);
        wetR *= T(1) / T(kVoices);
        l = l * (T(1) - T(0.5) * mix_) + wetL * mix_ * T(0.7);
        r = r * (T(1) - T(0.5) * mix_) + wetR * mix_ * T(0.7);
    }

private:
    static double tri(double p) {
        p -= math::floorD(p);
        return p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p;
    }

    double fs_ = 44100.0, rate_ = 1.0, depth_ = 2.0, phase_ = 0.0;
    T mix_ = T(0);
    DelayLine<T> left_, right_;
};

} // namespace spux::inline v1::dsp
