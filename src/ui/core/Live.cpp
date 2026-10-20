#include "spux/ui/Live.hpp"

#include "spux/dsp/Fft.hpp"

#include <algorithm>
#include <cmath>

namespace spux::inline v1::ui {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

Spectrum::Spectrum(const Options& o) : o_(o) {
    int n = 64; // the FFT needs a power of two
    while (n < o_.fftSize && n < (1 << 20))
        n <<= 1;
    o_.fftSize = n;
    o_.bands = std::max(1, o_.bands);
    samples_.assign(static_cast<std::size_t>(o_.fftSize), 0.0f);
    bands_.assign(static_cast<std::size_t>(o_.bands), 0.0f);
    fft_.assign(static_cast<std::size_t>(o_.fftSize), {});
    window_.resize(static_cast<std::size_t>(o_.fftSize));
    for (int i = 0; i < o_.fftSize; ++i)
        window_[static_cast<std::size_t>(i)] = 0.5 - 0.5 * std::cos(2.0 * kPi * i / (o_.fftSize - 1));
}

float Spectrum::frequencyAt(float t) const {
    return o_.lowHz * std::pow(o_.highHz / o_.lowHz, std::clamp(t, 0.0f, 1.0f));
}

void Spectrum::analyze(bool have, double sampleRate, float dt) {
    const float fall = std::exp(-dt * o_.releasePerSecond);
    if (!have) {
        for (float& b : bands_)
            b *= fall;
        return;
    }
    const int n = o_.fftSize;
    for (int i = 0; i < n; ++i) {
        const auto k = static_cast<std::size_t>(i);
        fft_[k] = {static_cast<double>(samples_[k]) * window_[k], 0.0};
    }
    spux::fft(fft_);
    const double sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    const double binHz = sr / n;
    for (int b = 0; b < o_.bands; ++b) {
        const float f0 = frequencyAt(static_cast<float>(b) / static_cast<float>(o_.bands));
        const float f1 = frequencyAt(static_cast<float>(b + 1) / static_cast<float>(o_.bands));
        const int k0 = std::max(1, static_cast<int>(std::floor(static_cast<double>(f0) / binHz)));
        const int k1 = std::min(n / 2 - 1, std::max(k0, static_cast<int>(std::ceil(static_cast<double>(f1) / binHz))));
        double peak = 0.0;
        for (int k = k0; k <= k1; ++k)
            peak = std::max(peak, std::abs(fft_[static_cast<std::size_t>(k)]));
        // a full-scale sine reads about n / 4 with a Hann window
        const double db = 20.0 * std::log10(peak / (n * 0.25) + 1e-9);
        const float v = static_cast<float>(std::clamp((db - o_.floorDb) / o_.rangeDb, 0.0, 1.0));
        float& cur = bands_[static_cast<std::size_t>(b)];
        cur = v > cur ? cur + (v - cur) * o_.attack : cur * fall + v * (1.0f - fall);
    }
}

} // namespace spux::inline v1::ui
