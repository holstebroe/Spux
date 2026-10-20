// In-place radix-2 complex FFT for analysis (spectrum views, tests, offline tools). The size must be a
// power of two. Not for the audio thread: it uses libm for the twiddle factors.
#pragma once

#include <cmath>
#include <complex>
#include <cstddef>
#include <utility>
#include <vector>

namespace spux::inline v1 {

// Forward: X[k] = sum x[n] e^(-2 pi i k n / N). Inverse uses the opposite sign and does not scale by 1 / N.
inline void fft(std::vector<std::complex<double>>& a, bool inverse = false) {
    constexpr double kPi = 3.14159265358979323846;
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double ang = (inverse ? 2.0 : -2.0) * kPi / static_cast<double>(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0);
            for (std::size_t k = 0; k < len / 2; ++k, w *= wl) {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
            }
        }
    }
}

} // namespace spux::inline v1
