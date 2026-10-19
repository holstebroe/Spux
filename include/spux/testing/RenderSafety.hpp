// Render safety check: every sample finite and the peak below full scale. Every factory preset and every
// reference render goes through it (a chord rendered at default settings must stay below 1).
#pragma once

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace spux::inline v1::testing {

struct SafetyReport {
    std::size_t samples = 0;
    std::size_t nonFinite = 0;      // NaN or infinity
    std::size_t firstNonFinite = 0; // index of the first one, when nonFinite > 0
    double peak = 0.0;              // largest |x| over the finite samples
    std::size_t peakIndex = 0;
    double maxPeak = 1.0; // the limit used: peak must be below it
    double minPeak = 0.0; // and, when > 0, at least this (a render that is silent is not safe either)

    bool ok() const { return nonFinite == 0 && peak < maxPeak && peak >= minPeak; }

    std::string describe() const {
        if (nonFinite > 0)
            return std::to_string(nonFinite) + " non-finite sample(s), the first at " + std::to_string(firstNonFinite);
        if (!(peak < maxPeak))
            return "peak " + std::to_string(peak) + " at " + std::to_string(peakIndex) + " is not below " +
                   std::to_string(maxPeak);
        if (peak < minPeak)
            return "peak " + std::to_string(peak) + " is below the minimum " + std::to_string(minPeak) + " (silent?)";
        return "ok, peak " + std::to_string(peak);
    }
};

// `x` may hold interleaved channels; the indices in the report are then sample indices, not frames.
inline SafetyReport checkRenderSafety(const float* x, std::size_t n, double maxPeak = 1.0, double minPeak = 0.0) {
    SafetyReport r;
    r.samples = n;
    r.maxPeak = maxPeak;
    r.minPeak = minPeak;
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isfinite(x[i])) {
            if (r.nonFinite++ == 0)
                r.firstNonFinite = i;
            continue;
        }
        const double a = std::fabs(static_cast<double>(x[i]));
        if (a > r.peak) {
            r.peak = a;
            r.peakIndex = i;
        }
    }
    return r;
}

inline SafetyReport checkRenderSafety(const std::vector<float>& x, double maxPeak = 1.0, double minPeak = 0.0) {
    return checkRenderSafety(x.data(), x.size(), maxPeak, minPeak);
}

// Stereo as two buffers: the report covers both (indices of the right channel follow the left's length).
inline SafetyReport checkRenderSafety(const std::vector<float>& l, const std::vector<float>& r, double maxPeak = 1.0,
                                      double minPeak = 0.0) {
    SafetyReport a = checkRenderSafety(l, maxPeak, minPeak);
    SafetyReport b = checkRenderSafety(r, maxPeak, minPeak);
    SafetyReport out = a;
    out.samples = a.samples + b.samples;
    if (b.nonFinite > 0 && a.nonFinite == 0)
        out.firstNonFinite = a.samples + b.firstNonFinite;
    out.nonFinite = a.nonFinite + b.nonFinite;
    if (b.peak > a.peak) {
        out.peak = b.peak;
        out.peakIndex = a.samples + b.peakIndex;
    }
    return out;
}

} // namespace spux::inline v1::testing
