// Audio-to-GUI telemetry: lock-free single-producer,
// single-consumer rings. The audio thread only writes, and only while an editor shows it (enabled); the
// GUI only reads. Nothing here allocates or locks after construction, so the audio side is real-time
// safe (R1). The frame type is the plugin's (voices, sources, peaks ...); it must be trivially copyable.
//
//   TelemetryRing<Frame, N>  frames, oldest first; a full ring drops the new frame (the reader is behind)
//   SampleRing<N>            the newest output samples, for a spectrum or a scope; overwritten freely
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "spux/Version.hpp"

namespace spux::inline v1 {

template <class Frame, std::size_t Capacity = 128> class TelemetryRing {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "capacity must be a power of two");
    static_assert(std::is_trivially_copyable_v<Frame>, "a telemetry frame is copied on the audio thread");

public:
    static constexpr std::size_t kCapacity = Capacity;

    // Editor side: the writer runs only while a window shows the data. Switching on drops what an earlier
    // window left unread; call it from the reading thread.
    void setEnabled(bool on) {
        if (on && !enabled())
            tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release);
        enabled_.store(on, std::memory_order_relaxed);
    }
    [[nodiscard]] bool enabled() const { return enabled_.load(std::memory_order_relaxed); }

    // --- audio thread ---
    // False when the reader is behind (the frame is dropped) or the ring is switched off.
    bool push(const Frame& f) {
        if (!enabled())
            return false;
        const std::size_t h = head_.load(std::memory_order_relaxed);
        if (h - tail_.load(std::memory_order_acquire) >= Capacity)
            return false;
        frames_[h & (Capacity - 1)] = f;
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    // --- GUI thread ---
    // The oldest frame not read yet; false when there is none.
    bool pop(Frame& out) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire))
            return false;
        out = frames_[t & (Capacity - 1)];
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }
    // Frames pushed so far (a change counter for change-driven redraws).
    [[nodiscard]] std::uint64_t pushed() const { return head_.load(std::memory_order_acquire); }

private:
    std::atomic<bool> enabled_{false};
    std::array<Frame, Capacity> frames_{};
    std::atomic<std::size_t> head_{0}, tail_{0};
};

template <std::size_t Capacity = (1u << 14)> class SampleRing {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "capacity must be a power of two");

public:
    static constexpr std::size_t kCapacity = Capacity;

    // Switching on starts a fresh window: samples written before it are not read (call it from the reading thread).
    void setEnabled(bool on) {
        if (on && !enabled())
            start_.store(head_.load(std::memory_order_acquire), std::memory_order_relaxed);
        enabled_.store(on, std::memory_order_relaxed);
    }
    [[nodiscard]] bool enabled() const { return enabled_.load(std::memory_order_relaxed); }

    // --- audio thread: the mono sum of a stereo block (r may be null for mono) ---
    void push(const float* l, const float* r, std::uint32_t n) {
        if (!enabled())
            return;
        std::size_t w = head_.load(std::memory_order_relaxed);
        for (std::uint32_t i = 0; i < n; ++i, ++w)
            samples_[w & (Capacity - 1)].store(r ? 0.5f * (l[i] + r[i]) : l[i], std::memory_order_relaxed);
        head_.store(w, std::memory_order_release);
    }

    // --- GUI thread ---
    // Copies the newest n samples (oldest first); false when fewer than n have been written. A read that
    // races the writer may see a few mixed samples: harmless for a display.
    bool read(float* dst, std::size_t n) const {
        const std::size_t w = head_.load(std::memory_order_acquire);
        if (n > Capacity / 2 || w - start_.load(std::memory_order_relaxed) < n)
            return false;
        for (std::size_t i = 0; i < n; ++i)
            dst[i] = samples_[(w - n + i) & (Capacity - 1)].load(std::memory_order_relaxed);
        return true;
    }
    [[nodiscard]] std::uint64_t written() const { return head_.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> enabled_{false};
    std::array<std::atomic<float>, Capacity> samples_{};
    std::atomic<std::size_t> head_{0}, start_{0};
};

} // namespace spux::inline v1
