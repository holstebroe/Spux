// ITU-R BS.1770 loudness for tests and offline tools: K-weighting (the published 48 kHz coefficients, so
// callers render at 48 kHz), momentary loudness (400 ms windows, 100 ms hop), gated integrated loudness
// and the sample peak. Offline only; allocates freely.
//
// A plugin's preset phrases and gain fit stay in the plugin.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace spux::inline v1::testing {

// One K-weighting channel: stage 1 high shelf, stage 2 RLB high-pass (48 kHz).
struct KWeighting {
    struct Biquad {
        double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
        double run(double x) {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };
    Biquad shelf{1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585};
    Biquad high{1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621};

    double tick(double x) { return high.run(shelf.run(x)); }
};

inline constexpr int kLoudnessRate = 48000;

inline double lufsOf(double meanSquare) { return -0.691 + 10.0 * std::log10(meanSquare); }

// Max momentary loudness (LUFS) of a stereo signal at 48 kHz: the loudest 400 ms window. A signal
// shorter than one window, or silence, reads -120.7 LUFS.
inline double maxMomentaryLufs(const std::vector<float>& l, const std::vector<float>& r) {
    const std::size_t n = std::min(l.size(), r.size());
    const std::size_t win = kLoudnessRate * 4 / 10, hop = kLoudnessRate / 10;
    KWeighting kl, kr;
    std::vector<double> e(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double a = kl.tick(l[i]), b = kr.tick(r[i]);
        e[i] = a * a + b * b;
    }
    double best = 1e-12;
    for (std::size_t start = 0; start + win <= e.size(); start += hop) {
        double s = 0.0;
        for (std::size_t i = 0; i < win; ++i)
            s += e[start + i];
        best = std::max(best, s / static_cast<double>(win));
    }
    return lufsOf(best);
}

struct LoudnessResult {
    double integratedLufs = -100.0; // -100 when nothing passes the absolute gate
    double peakDb = -200.0;         // sample peak, dBFS
    bool finite = true;             // false when a sample was NaN or infinite (it is left out of the numbers)
};

// BS.1770-4 integrated loudness (400 ms blocks with 75 % overlap, absolute gate -70 LUFS, relative gate
// -10 LU) of a stereo signal at 48 kHz, plus the sample peak.
inline LoudnessResult measureLoudness(const std::vector<float>& l, const std::vector<float>& r) {
    const std::size_t n = std::min(l.size(), r.size());
    LoudnessResult out;
    double peak = 0.0;
    std::vector<double> energy(n); // K-weighted power per sample, summed over the channels
    for (int ch = 0; ch < 2; ++ch) {
        const std::vector<float>& x = ch == 0 ? l : r;
        KWeighting k;
        for (std::size_t i = 0; i < n; ++i) {
            if (!std::isfinite(x[i])) {
                out.finite = false;
                continue;
            }
            peak = std::max(peak, static_cast<double>(std::fabs(x[i])));
            const double y = k.tick(x[i]);
            if (ch == 0)
                energy[i] = y * y;
            else
                energy[i] += y * y;
        }
    }
    out.peakDb = peak > 0.0 ? 20.0 * std::log10(peak) : -200.0;
    const std::size_t block = kLoudnessRate * 4 / 10, hop = kLoudnessRate / 10;
    std::vector<double> power;
    for (std::size_t s = 0; s + block <= n; s += hop) {
        double sum = 0.0; // recomputed per block: no drift from running sums
        for (std::size_t i = s; i < s + block; ++i)
            sum += energy[i];
        power.push_back(sum / static_cast<double>(block));
    }
    double acc = 0.0;
    std::size_t cnt = 0;
    for (double p : power)
        if (p > 0.0 && lufsOf(p) > -70.0) {
            acc += p;
            ++cnt;
        }
    if (cnt == 0)
        return out;
    const double gate = lufsOf(acc / static_cast<double>(cnt)) - 10.0;
    acc = 0.0;
    cnt = 0;
    for (double p : power)
        if (p > 0.0 && lufsOf(p) > gate) {
            acc += p;
            ++cnt;
        }
    if (cnt > 0)
        out.integratedLufs = lufsOf(acc / static_cast<double>(cnt));
    return out;
}

} // namespace spux::inline v1::testing
