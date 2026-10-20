// Generic consumers of telemetry for live views (guidelines 7 "Telemetry").
// They run on the editor's thread only, once per frame. What is specific to a plugin (its sources,
// attractor trails, voices) stays in the plugin and feeds these.
//
//   LevelMeter   instant attack, exponential release per update (peak meters, gain reduction)
//   History<N>   the last N values of a signal, oldest first (source traces)
//   Spectrum     a log-spaced skyline from a SampleRing: Hann window, FFT, the peak per band in dB mapped
//                to 0..1, fast attack and slow release
#pragma once

#include <array>
#include <complex>
#include <cstddef>
#include <vector>

#include "spux/core/TelemetryRing.hpp"

namespace spux::inline v1::ui {

class LevelMeter {
public:
    explicit LevelMeter(float release = 0.9f) : release_(release) {}
    // One update with the newest peak: rises at once, falls by `release` per update.
    void update(float peak) { level_ = peak > level_ * release_ ? peak : level_ * release_; }
    void reset() { level_ = 0.0f; }
    [[nodiscard]] float level() const { return level_; }

private:
    float release_;
    float level_ = 0.0f;
};

template <std::size_t N> class History {
public:
    void push(float v) {
        values_[head_] = v;
        head_ = (head_ + 1) % N;
        if (filled_ < N)
            ++filled_;
    }
    [[nodiscard]] std::size_t count() const { return filled_; }
    // i = 0 is the oldest value kept.
    [[nodiscard]] float operator[](std::size_t i) const {
        return i < filled_ ? values_[(head_ + N - filled_ + i) % N] : 0.0f;
    }
    // The newest values, at most maxCount, copied oldest first; returns how many were written.
    std::size_t copyTo(float* out, std::size_t maxCount) const {
        const std::size_t n = filled_ < maxCount ? filled_ : maxCount;
        for (std::size_t i = 0; i < n; ++i)
            out[i] = (*this)[filled_ - n + i];
        return n;
    }
    void clear() { head_ = filled_ = 0; }

private:
    std::array<float, N> values_{};
    std::size_t head_ = 0, filled_ = 0;
};

class Spectrum {
public:
    struct Options {
        int fftSize = 2048; // power of two
        int bands = 96;
        float lowHz = 30.0f, highHz = 18000.0f;
        float floorDb = -84.0f; // maps to 0
        float rangeDb = 72.0f;  // floorDb + rangeDb maps to 1
        float attack = 0.6f;    // fraction of a rise taken per update
        float releasePerSecond = 6.0f;
    };
    Spectrum() : Spectrum(Options{}) {}
    explicit Spectrum(const Options& o);

    // One update from the newest samples; `active` false lets the bands fall (nothing is playing).
    template <std::size_t C> void update(const SampleRing<C>& ring, double sampleRate, float dt, bool active = true) {
        const bool have = active && ring.read(samples_.data(), samples_.size());
        analyze(have, sampleRate, dt);
    }
    // 0..1 per band.
    [[nodiscard]] const std::vector<float>& bands() const { return bands_; }
    // The frequency at a position 0..1 across the bands.
    [[nodiscard]] float frequencyAt(float t) const;

private:
    void analyze(bool have, double sampleRate, float dt);

    Options o_;
    std::vector<float> samples_, bands_;
    std::vector<std::complex<double>> fft_;
    std::vector<double> window_; // Hann, computed once
};

} // namespace spux::inline v1::ui
