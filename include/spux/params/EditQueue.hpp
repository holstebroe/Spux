// Parameter edits made in the editor on their way to the host (from Plumeflow `plugin/EditQueue`). The
// editor writes the value into the ParamStore (the sound follows at once) and queues the change here;
// the audio thread (process) or the host's flush turns the queue into gesture-begin, value and
// gesture-end events, so the host records automation and undo (spux/clap/ClapHelpers.hpp, drainEdits).
//
// Single producer, single consumer, lock-free. Several producer threads (an editor thread and the main
// thread) must serialize their pushes themselves; the consumer never waits for them.
//
// Value updates leave kGestureReserve slots free, so the gesture marks of a fast drag still fit when
// values fill the queue: a dropped value costs nothing (the store has it), a dropped gesture end leaves
// the host's gesture open. Use pushBegin / pushValue / pushEnd, or tryPush with a reserve of your own.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "spux/Version.hpp"

namespace spux::inline v1::params {

struct ParamEdit {
    enum class Kind : std::uint8_t { Begin, Value, End };
    Kind kind = Kind::Value;
    std::uint32_t index = 0; // ParamStore / ParamTable index
    double host = 0.0;       // host value, for Value
};

template <std::size_t Capacity = 1024, std::size_t GestureReserve = 64> class BasicEditQueue {
    static_assert(Capacity >= 4 && (Capacity & (Capacity - 1)) == 0, "capacity must be a power of two");
    static_assert(GestureReserve + 2 < Capacity, "the gesture reserve must leave room for values");

public:
    static constexpr std::size_t kCapacity = Capacity;
    static constexpr std::size_t kGestureReserve = GestureReserve;

    // False when fewer than `reserve` + 1 slots are free.
    bool tryPush(const ParamEdit& e, std::size_t reserve = 0) {
        const std::size_t h = head_.load(std::memory_order_relaxed);
        if (h - tail_.load(std::memory_order_acquire) + reserve >= kCapacity)
            return false;
        ring_[h & (kCapacity - 1)] = e;
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    // A begin keeps one slot for its end; a begin that does not fit should drop its end too, so the
    // host never sees an unbalanced gesture (Plumeflow's Gui does this).
    bool pushBegin(std::uint32_t index) { return tryPush({ParamEdit::Kind::Begin, index, 0.0}, 1); }
    bool pushValue(std::uint32_t index, double host) {
        return tryPush({ParamEdit::Kind::Value, index, host}, kGestureReserve);
    }
    bool pushEnd(std::uint32_t index) { return tryPush({ParamEdit::Kind::End, index, 0.0}); }

    // Producer side: slots free now (the consumer may free more meanwhile, never fewer).
    [[nodiscard]] std::size_t freeSlots() const {
        return kCapacity - (head_.load(std::memory_order_relaxed) - tail_.load(std::memory_order_acquire));
    }

    bool pop(ParamEdit& e) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire))
            return false;
        e = ring_[t & (kCapacity - 1)];
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }
    [[nodiscard]] bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

private:
    std::array<ParamEdit, kCapacity> ring_{};
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
};

using EditQueue = BasicEditQueue<>;

} // namespace spux::inline v1::params
