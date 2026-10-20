// Shared helpers of the core and DSP tests: deterministic stimuli, alias measurement and the kernel
// reference check (guidelines section 3: each block has short reference outputs and a golden hash).
#pragma once

#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "spux/core/Math.hpp"
#include "spux/core/Pcg32.hpp"
#include "spux/testing/Hash.hpp"
#include "spux/testing/Metrics.hpp"

namespace spux_test {

inline constexpr double kFs = 48000.0;

// A test signal with tones, a sweep, noise, steps and silence: 4096 samples at 48 kHz in [-1, 1].
inline std::vector<float> stimulus(int n = 4096) {
    std::vector<float> x(static_cast<size_t>(n));
    spux::Pcg32 rng(2026, 10);
    double sweepPhase = 0.0;
    for (int i = 0; i < n; ++i) {
        const double t = i / kFs;
        sweepPhase += (40.0 + 12000.0 * i / n) / kFs;
        double v =
            0.35 * std::sin(2.0 * spux::math::kPi * 220.0 * t) + 0.25 * std::sin(2.0 * spux::math::kPi * sweepPhase);
        v += 0.15 * rng.nextBipolar();
        if ((i / 512) % 2 == 1)
            v += 0.2; // steps
        if (i >= n - 256)
            v = 0.0; // a silent tail
        x[static_cast<size_t>(i)] = static_cast<float>(v);
    }
    return x;
}

// Ratio (dB) of the power outside the harmonic bins m k to the power in them, for a periodic signal of
// N samples whose fundamental sits exactly on bin k (k odd, so no alias lands on a harmonic bin).
inline double aliasDb(const std::vector<float>& y, size_t k) {
    std::vector<std::complex<double>> a(y.begin(), y.end());
    spux::testing::fft(a);
    double harm = 0.0, alias = 0.0;
    for (size_t b = 1; b < a.size() / 2; ++b) {
        const double p = std::norm(a[b]);
        (b % k == 0 ? harm : alias) += p;
    }
    return 10.0 * std::log10(alias / harm);
}

inline std::vector<float> toFloat(const std::vector<double>& x) {
    std::vector<float> y;
    y.reserve(x.size());
    for (double v : x)
        y.push_back(static_cast<float>(v));
    return y;
}

inline std::string dataDir() { return SPUX_TEST_DATA_DIR; }

// The kernel reference of one block: the golden hash of the whole output, plus its first 64 values as
// text (readable when the hash moves, and comparable within 1e-6 across toolchains). The hashes live in
// tests/kernels/<platform>.txt when a toolchain needs its own, else in tests/kernels/portable.txt (the
// blocks call no libm, so one set normally serves every platform). SPUX_UPDATE_GOLDEN=1 records both.
class KernelReferences {
public:
    KernelReferences() : golden_(hashPath()) {}
    ~KernelReferences() { golden_.save(); }

    void check(const std::string& name, const std::vector<float>& y) {
        INFO("kernel " << name);
        for (float v : y)
            REQUIRE(std::isfinite(v));
        const std::string valuesPath = dataDir() + "/kernels/" + name + ".txt";
        const size_t count = std::min<size_t>(64, y.size());
        if (golden_.updating()) {
            std::ofstream out(valuesPath, std::ios::trunc);
            out << "# first " << count << " outputs of kernel " << name << " (tests/dsp_kernel_test.cpp)\n";
            char buf[48];
            for (size_t i = 0; i < count; ++i) {
                std::snprintf(buf, sizeof(buf), "%.9g\n", static_cast<double>(y[i]));
                out << buf;
            }
        } else {
            std::ifstream in(valuesPath);
            REQUIRE_MESSAGE(in.good(), "no reference values " << valuesPath << "; run with SPUX_UPDATE_GOLDEN=1");
            std::string line;
            size_t i = 0;
            while (std::getline(in, line) && i < count) {
                if (line.empty() || line[0] == '#')
                    continue;
                const double ref = std::stod(line);
                const double tol = 1e-6 * std::max(1.0, std::fabs(ref));
                CHECK_MESSAGE(std::fabs(static_cast<double>(y[i]) - ref) <= tol,
                              "sample " << i << ": " << y[i] << " vs reference " << ref);
                ++i;
            }
            CHECK(i == count);
        }
        const auto hash = spux::testing::hashFloats(y);
        const auto r = golden_.verify(name, hash);
        CHECK_MESSAGE(
            (r == spux::testing::GoldenFile::Result::Match || r == spux::testing::GoldenFile::Result::Updated),
            golden_.describe(name, hash, r));
    }

private:
    static std::string hashPath() {
        const std::string own = spux::testing::goldenPath(dataDir() + "/kernels");
        return std::filesystem::exists(own) ? own : dataDir() + "/kernels/portable.txt";
    }
    spux::testing::GoldenFile golden_;
};

} // namespace spux_test
