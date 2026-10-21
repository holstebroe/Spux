// Which voice plays a note. The plugin owns the voices; the allocator owns the bookkeeping of who plays what.
//
// noteOn(key) picks a slot among the first `limit`:
//   1. the voice already playing this key (held, or still ringing out): a re-struck key keeps its voice;
//   2. a free voice, searched round-robin from the one after the last given;
//   3. the voice released longest ago;
//   4. the oldest held note (stolen).
// The result says whether it was a retrigger or a steal and whether the slot was held before, which is what a
// mono voice needs for legato (limit 1, `wasHeld`: glide instead of retriggering). noteOff(key) releases the
// held voice of that key; the plugin calls free(slot) when a voice has finished sounding (its envelope is idle).
// Order counters make the choices deterministic (R3); nothing here allocates.
#pragma once

#include <array>
#include <cstdint>

namespace spux::inline v1::dsp {

class VoiceAllocator {
public:
    static constexpr int kMaxVoices = 32;

    struct Slot {
        int key = -1;
        bool held = false;     // the key is down
        bool sounding = false; // the voice has not finished
        std::uint64_t onOrder = 0, offOrder = 0;
    };
    struct Result {
        int slot = -1;
        bool retrigger = false; // the slot already played this key
        bool stolen = false;    // the slot was sounding another note
        bool wasHeld = false;   // the slot's previous note was still held
    };

    // The number of voices in use, 1 to kMaxVoices. Slots beyond it are not given out; notes still held there are
    // the plugin's to release.
    void setLimit(int n) { limit_ = n < 1 ? 1 : n > kMaxVoices ? kMaxVoices : n; }
    [[nodiscard]] int limit() const { return limit_; }

    void reset() {
        slots_ = {};
        counter_ = 0;
        next_ = 0;
    }

    Result noteOn(int key) {
        Result r;
        r.slot = find(key);
        if (r.slot >= 0) {
            r.retrigger = true;
        } else {
            for (int k = 0; k < limit_ && r.slot < 0; ++k) {
                const int i = (next_ + k) % limit_;
                if (!slots_[static_cast<std::size_t>(i)].held && !slots_[static_cast<std::size_t>(i)].sounding)
                    r.slot = i;
            }
            if (r.slot >= 0) {
                next_ = (r.slot + 1) % limit_;
            } else {
                r.slot = steal();
                r.stolen = true;
            }
        }
        Slot& s = slots_[static_cast<std::size_t>(r.slot)];
        r.wasHeld = s.held;
        s.key = key;
        s.held = true;
        s.sounding = true;
        s.onOrder = ++counter_;
        return r;
    }

    // Releases the held voice playing `key`; its slot, or -1 when none.
    int noteOff(int key) {
        for (int i = 0; i < kMaxVoices; ++i) {
            Slot& s = slots_[static_cast<std::size_t>(i)];
            if (s.held && s.key == key) {
                release(i);
                return i;
            }
        }
        return -1;
    }
    void release(int slot) {
        Slot& s = slots_[static_cast<std::size_t>(slot)];
        if (!s.held)
            return;
        s.held = false;
        s.offOrder = ++counter_;
    }
    // The voice has finished sounding.
    void free(int slot) {
        Slot& s = slots_[static_cast<std::size_t>(slot)];
        s.held = false;
        s.sounding = false;
        s.key = -1;
    }
    [[nodiscard]] const Slot& slot(int i) const { return slots_[static_cast<std::size_t>(i)]; }

private:
    [[nodiscard]] int find(int key) const {
        for (int i = 0; i < limit_; ++i) {
            const Slot& s = slots_[static_cast<std::size_t>(i)];
            if (s.key == key && (s.held || s.sounding))
                return i;
        }
        return -1;
    }
    [[nodiscard]] int steal() const {
        int best = -1;
        for (int i = 0; i < limit_; ++i) {
            const Slot& s = slots_[static_cast<std::size_t>(i)];
            if (!s.held && (best < 0 || s.offOrder < slots_[static_cast<std::size_t>(best)].offOrder))
                best = i;
        }
        if (best >= 0)
            return best;
        best = 0;
        for (int i = 1; i < limit_; ++i)
            if (slots_[static_cast<std::size_t>(i)].onOrder < slots_[static_cast<std::size_t>(best)].onOrder)
                best = i;
        return best;
    }

    std::array<Slot, kMaxVoices> slots_{};
    std::uint64_t counter_ = 0;
    int limit_ = 8, next_ = 0;
};

} // namespace spux::inline v1::dsp
