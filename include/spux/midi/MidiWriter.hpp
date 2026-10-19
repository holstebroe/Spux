// Standard MIDI File writer: tracks are collected as events at absolute ticks and encoded to delta times
// at the end, so a caller can add them in any order. At one tick the order is: priority, then the order
// of adding. The default priorities put note-offs before everything, meta events and controllers next,
// then note-ons, then pressure, so a note that ends where another starts never swallows it.
//
// Merged from Acidus (sequencer/MidiExport.cpp: a format 0 clip with a name, 4/4 and pressure) and
// Paulascape (export/midi_export.cpp: a conductor track and channel tracks with a tempo map). What
// decides which notes to write stays in the plugin.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

namespace spux::inline v1::midi {

namespace priority {
inline constexpr int kNoteOff = 0; // also pitch bend: a bend belongs before the controllers at its tick
inline constexpr int kMeta = 1;    // meta events, program changes, controllers
inline constexpr int kNoteOn = 2;
inline constexpr int kPressure = 3; // polyphonic pressure applies to notes already held
} // namespace priority

inline void putVarLen(std::vector<std::uint8_t>& out, std::uint32_t v) {
    if (v > 0x0FFFFFFFu)
        v = 0x0FFFFFFFu; // the largest a variable-length quantity can hold
    std::uint8_t tmp[5];
    int n = 0;
    tmp[n++] = v & 0x7F;
    while ((v >>= 7) != 0)
        tmp[n++] = static_cast<std::uint8_t>(0x80 | (v & 0x7F));
    while (n > 0)
        out.push_back(tmp[--n]);
}

inline void putBigEndian(std::vector<std::uint8_t>& out, std::uint32_t v, int bytes) {
    for (int i = bytes - 1; i >= 0; --i)
        out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}

class TrackWriter {
public:
    // Raw event bytes: a channel message with its status, or {0xFF, type, length-prefixed payload...}.
    void add(std::uint32_t tick, int priority, std::vector<std::uint8_t> bytes) {
        events_.push_back({tick, priority, events_.size(), std::move(bytes)});
    }

    void noteOn(std::uint32_t tick, int channel, int key, int velocity, int prio = priority::kNoteOn) {
        add(tick, prio, {status(0x90, channel), data(key), data(velocity)});
    }
    void noteOff(std::uint32_t tick, int channel, int key, int velocity = 64, int prio = priority::kNoteOff) {
        add(tick, prio, {status(0x80, channel), data(key), data(velocity)});
    }
    void pressure(std::uint32_t tick, int channel, int key, int value, int prio = priority::kPressure) {
        add(tick, prio, {status(0xA0, channel), data(key), data(value)});
    }
    void controller(std::uint32_t tick, int channel, int number, int value, int prio = priority::kMeta) {
        add(tick, prio, {status(0xB0, channel), data(number), data(value)});
    }
    void program(std::uint32_t tick, int channel, int number, int prio = priority::kMeta) {
        add(tick, prio, {status(0xC0, channel), data(number)});
    }
    // value 0..16383, 8192 is the centre.
    void pitchBend(std::uint32_t tick, int channel, int value, int prio = priority::kNoteOff) {
        add(tick, prio, {status(0xE0, channel), data(value & 127), data((value >> 7) & 127)});
    }

    void meta(std::uint32_t tick, std::uint8_t type, const std::vector<std::uint8_t>& payload,
              int prio = priority::kMeta) {
        std::vector<std::uint8_t> b{0xFF, type};
        putVarLen(b, static_cast<std::uint32_t>(payload.size()));
        b.insert(b.end(), payload.begin(), payload.end());
        add(tick, prio, std::move(b));
    }
    void name(const std::string& text, std::uint32_t tick = 0) { meta(tick, 0x03, {text.begin(), text.end()}); }
    void tempo(std::uint32_t tick, std::uint32_t microsPerQuarter) {
        meta(tick, 0x51,
             {static_cast<std::uint8_t>(microsPerQuarter >> 16), static_cast<std::uint8_t>(microsPerQuarter >> 8),
              static_cast<std::uint8_t>(microsPerQuarter)});
    }
    // numerator / 2^denominatorPower, e.g. 4/4 is (4, 2); clocksPerClick 24 and 8 thirty-seconds per quarter
    // are the usual values.
    void timeSignature(std::uint32_t tick, int numerator, int denominatorPower, int clocksPerClick = 24,
                       int thirtySecondsPerQuarter = 8) {
        meta(tick, 0x58,
             {static_cast<std::uint8_t>(numerator), static_cast<std::uint8_t>(denominatorPower),
              static_cast<std::uint8_t>(clocksPerClick), static_cast<std::uint8_t>(thirtySecondsPerQuarter)});
    }

    // The track ends at this tick, or at its last event if that is later (the clip length of a pattern).
    void setEndTick(std::uint32_t tick) { endTick_ = tick; }
    std::size_t size() const { return events_.size(); }

    // Appends the MTrk chunk.
    void encode(std::vector<std::uint8_t>& out) const {
        std::vector<Event> sorted = events_;
        std::stable_sort(sorted.begin(), sorted.end(), [](const Event& a, const Event& b) {
            if (a.tick != b.tick)
                return a.tick < b.tick;
            if (a.priority != b.priority)
                return a.priority < b.priority;
            return a.seq < b.seq;
        });
        std::vector<std::uint8_t> body;
        std::uint32_t last = 0;
        for (const Event& e : sorted) {
            putVarLen(body, e.tick - last);
            last = e.tick;
            body.insert(body.end(), e.bytes.begin(), e.bytes.end());
        }
        putVarLen(body, std::max(endTick_, last) - last);
        body.insert(body.end(), {0xFF, 0x2F, 0x00});
        out.insert(out.end(), {'M', 'T', 'r', 'k'});
        putBigEndian(out, static_cast<std::uint32_t>(body.size()), 4);
        out.insert(out.end(), body.begin(), body.end());
    }

private:
    struct Event {
        std::uint32_t tick;
        int priority;
        std::size_t seq;
        std::vector<std::uint8_t> bytes;
    };
    static std::uint8_t status(int kind, int channel) { return static_cast<std::uint8_t>(kind | (channel & 0x0F)); }
    static std::uint8_t data(int v) { return static_cast<std::uint8_t>(std::clamp(v, 0, 127)); }

    std::vector<Event> events_;
    std::uint32_t endTick_ = 0;
};

class FileWriter {
public:
    // format 0: one track; 1: a conductor track plus others, played together.
    explicit FileWriter(int format = 1, int division = 96) : format_(format), division_(division) {}

    // The reference stays valid while more tracks are added.
    TrackWriter& addTrack() { return tracks_.emplace_back(); }

    std::vector<std::uint8_t> bytes() const {
        std::vector<std::uint8_t> out{'M', 'T', 'h', 'd'};
        putBigEndian(out, 6, 4);
        putBigEndian(out, static_cast<std::uint32_t>(format_), 2);
        putBigEndian(out, static_cast<std::uint32_t>(tracks_.size()), 2);
        putBigEndian(out, static_cast<std::uint32_t>(division_), 2);
        for (const TrackWriter& t : tracks_)
            t.encode(out);
        return out;
    }

    bool write(const std::string& path) const {
        const std::vector<std::uint8_t> b = bytes();
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f)
            return false;
        const bool ok = std::fwrite(b.data(), 1, b.size(), f) == b.size();
        return std::fclose(f) == 0 && ok;
    }

private:
    int format_;
    int division_;
    std::deque<TrackWriter> tracks_;
};

} // namespace spux::inline v1::midi
