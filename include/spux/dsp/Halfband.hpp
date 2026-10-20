// Zero-latency oversampling with cascaded IIR half-band stages. Each stage is a polyphase pair of
// first-order all-pass chains,
//     H(z) = 0.5 (A0(z^2) + z^-1 A1(z^2)),
// so neither direction adds a fixed delay to compensate (group delay is a few samples near the band
// edge).
//
// Design: the closed-form elliptic polyphase half-band (Valenzuela and Constantinides, in the form of
// Laurent de Soras' hiir), 5 + 5 all-pass sections (order 21), transition band 0.45 to 0.55 of the
// low rate. Measured on the float implementation (tests/halfband_test.cpp, docs/BENCH.md): stopband
// -108.2 dB, passband ripple below 1e-5 dB. Chosen over 4 + 4 sections (-95.2 dB, about 25 %
// cheaper): -95 dB images of a full-scale signal sit only 5 dB under the -90 dBFS null threshold
// and above 16-bit resolution, while -108 dB clears both and comes within 2 dB of the strict -110 dBFS
// gate; the cost is a few ns per sample and stage (docs/BENCH.md).
//
// Sample type T: float for one channel, or any type with + and * against T(float) (a SIMD lane type).
#pragma once

#include <array>
#include <cstddef>

namespace spux::inline v1::dsp {

struct HalfbandDesign {
    static constexpr std::array<float, 5> kA0{0.03433275086513824f, 0.2604732461798622f, 0.5431863228953023f,
                                              0.7630482744643935f, 0.9112315917121574f};
    static constexpr std::array<float, 5> kA1{0.1284817903576754f, 0.4052449616132795f, 0.6634670568987546f,
                                              0.8440242987628593f, 0.9708498575390465f};
};

// One 2x stage. A stage is stateful: an instance serves one direction (up or down) of one channel.
template <class T = float> class HalfbandStage {
public:
    void reset() {
        a0_.fill({});
        a1_.fill({});
    }

    // One input at rate fs gives two outputs at 2 fs (the earlier one first). Gain 1.
    void upsample(T x, T& first, T& second) {
        first = runChain(HalfbandDesign::kA0, a0_, x);
        second = runChain(HalfbandDesign::kA1, a1_, x);
    }

    // Two inputs at 2 fs (the earlier one first) give one output at fs.
    [[nodiscard]] T downsample(T earlier, T later) {
        const T y0 = runChain(HalfbandDesign::kA0, a0_, later);
        const T y1 = runChain(HalfbandDesign::kA1, a1_, earlier);
        return T(0.5f) * (y0 + y1);
    }

private:
    struct Section {
        T x1{};
        T y1{};
    };

    template <std::size_t N> static T runChain(const std::array<float, N>& coef, std::array<Section, N>& state, T x) {
        // First-order all-pass per section: y = a x + x1 - a y1.
        T v = x;
        for (std::size_t i = 0; i < N; ++i) {
            Section& s = state[i];
            const T a(coef[i]);
            const T y = a * v + s.x1 - a * s.y1;
            s.x1 = v;
            s.y1 = y;
            v = y;
        }
        return v;
    }

    std::array<Section, HalfbandDesign::kA0.size()> a0_{};
    std::array<Section, HalfbandDesign::kA1.size()> a1_{};
};

// Up- and downsampling by 1, 2, 4 or 8, chosen at run time without allocating: log2(factor) stages per
// direction, each running at its own rate in time order.
template <class T = float> class Oversampler {
public:
    static constexpr int kMaxFactor = 8;

    // factor 1, 2, 4 or 8 (others are rounded up to the next power of two, at most 8). Clears the state.
    void setFactor(int factor) {
        stages_ = 0;
        while ((1 << stages_) < factor && stages_ < kMaxStages)
            ++stages_;
        factor_ = 1 << stages_;
        reset();
    }
    [[nodiscard]] int factor() const { return factor_; }

    void reset() {
        for (auto& s : up_)
            s.reset();
        for (auto& s : down_)
            s.reset();
    }

    // Writes factor() samples at the oversampled rate for one input sample.
    void upsample(T x, T* out) {
        // Unrolled 2x and 4x: the same stages in the same order as the general loop below.
        if (stages_ == 1) {
            up_[0].upsample(x, out[0], out[1]);
            return;
        }
        if (stages_ == 2) {
            T a, b;
            up_[0].upsample(x, a, b);
            up_[1].upsample(a, out[0], out[1]);
            up_[1].upsample(b, out[2], out[3]);
            return;
        }
        std::array<T, kMaxFactor> buf{};
        buf[0] = x;
        int count = 1;
        for (int s = 0; s < stages_; ++s) {
            std::array<T, kMaxFactor> next{};
            for (int i = 0; i < count; ++i)
                up_[static_cast<std::size_t>(s)].upsample(buf[static_cast<std::size_t>(i)],
                                                          next[static_cast<std::size_t>(2 * i)],
                                                          next[static_cast<std::size_t>(2 * i + 1)]);
            count *= 2;
            buf = next;
        }
        for (int i = 0; i < factor_; ++i)
            out[i] = buf[static_cast<std::size_t>(i)];
    }

    // Consumes factor() samples at the oversampled rate and returns one output sample.
    [[nodiscard]] T downsample(const T* in) {
        if (stages_ == 1)
            return down_[0].downsample(in[0], in[1]);
        if (stages_ == 2) {
            const T a = down_[1].downsample(in[0], in[1]);
            const T b = down_[1].downsample(in[2], in[3]);
            return down_[0].downsample(a, b);
        }
        std::array<T, kMaxFactor> buf{};
        for (int i = 0; i < factor_; ++i)
            buf[static_cast<std::size_t>(i)] = in[i];
        int count = factor_;
        for (int s = stages_ - 1; s >= 0; --s) {
            for (int i = 0; i < count / 2; ++i)
                buf[static_cast<std::size_t>(i)] = down_[static_cast<std::size_t>(s)].downsample(
                    buf[static_cast<std::size_t>(2 * i)], buf[static_cast<std::size_t>(2 * i + 1)]);
            count /= 2;
        }
        return buf[0];
    }

    // One host-rate sample through core(x) at the oversampled rate (core is called factor() times, in
    // time order, and may keep state).
    template <class Core> T process(T x, Core&& core) {
        std::array<T, kMaxFactor> buf{};
        upsample(x, buf.data());
        for (int i = 0; i < factor_; ++i)
            buf[static_cast<std::size_t>(i)] = core(buf[static_cast<std::size_t>(i)]);
        return downsample(buf.data());
    }

    // Blocks: up() writes n * factor() samples, down() reads them back into n samples.
    void up(const T* in, int n, T* out) {
        for (int i = 0; i < n; ++i)
            upsample(in[i], out + static_cast<std::ptrdiff_t>(i) * factor_);
    }
    void down(const T* in, int n, T* out) {
        for (int i = 0; i < n; ++i)
            out[i] = downsample(in + static_cast<std::ptrdiff_t>(i) * factor_);
    }

private:
    static constexpr int kMaxStages = 3;
    std::array<HalfbandStage<T>, kMaxStages> up_{};
    std::array<HalfbandStage<T>, kMaxStages> down_{};
    int factor_ = 1;
    int stages_ = 0;
};

} // namespace spux::inline v1::dsp
