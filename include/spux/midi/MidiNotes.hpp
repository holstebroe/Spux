// From a parsed MidiFile to what the offline tools need: the tempo map (tick to seconds), timed note
// events in file order for a renderer, and paired notes with a start and an end.
#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "spux/midi/MidiFile.hpp"

namespace spux::inline v1::midi {

// Tempo changes of every track. The default is 120 bpm until the first change; two changes at one tick:
// the later one in the file wins.
class TempoMap {
public:
    explicit TempoMap(const MidiFile& file) : division_(file.division > 0 ? file.division : 480) {
        for (const MidiEvent& e : file.events)
            if (e.metaType() == 0x51 && e.data.size() == 5)
                changes_.push_back(
                    {e.tick, static_cast<std::uint32_t>((e.data[2] << 16) | (e.data[3] << 8) | e.data[4])});
        std::stable_sort(changes_.begin(), changes_.end(),
                         [](const Change& a, const Change& b) { return a.tick < b.tick; });
    }

    double seconds(std::uint32_t tick) const {
        double s = 0.0;
        std::uint32_t last = 0;
        double micros = 500000.0;
        for (const Change& c : changes_) {
            if (c.tick > tick)
                break;
            s += static_cast<double>(c.tick - last) * micros * 1e-6 / division_;
            last = c.tick;
            micros = static_cast<double>(c.micros);
        }
        return s + static_cast<double>(tick - last) * micros * 1e-6 / division_;
    }

private:
    struct Change {
        std::uint32_t tick;
        std::uint32_t micros; // per quarter note
    };
    std::vector<Change> changes_;
    int division_;
};

struct NoteEvent {
    double seconds = 0.0;
    std::uint32_t tick = 0;
    bool on = true;
    int key = 60;
    int channel = 0;
    float velocity = 1.0f; // 0..1; 0 for a note-off
};

// Note-on and note-off events sorted by time; simultaneous events keep file order (stable), so a
// writer that puts note-offs first keeps them first.
inline std::vector<NoteEvent> noteEvents(const MidiFile& file) {
    const TempoMap tempo(file);
    std::vector<NoteEvent> out;
    for (const MidiEvent& e : file.events) {
        if (!e.isNoteOn() && !e.isNoteOff())
            continue;
        const bool on = e.isNoteOn();
        out.push_back({tempo.seconds(e.tick), e.tick, on, e.data[1], e.channel(),
                       on ? static_cast<float>(e.data[2]) / 127.0f : 0.0f});
    }
    std::stable_sort(out.begin(), out.end(), [](const NoteEvent& a, const NoteEvent& b) { return a.tick < b.tick; });
    return out;
}

struct Note {
    double start = 0.0, end = 0.0; // seconds
    int key = 60;
    int channel = 0;
    int velocity = 100; // 1..127
};

// Paired notes sorted by start. A second note-on for a key that is still sounding ends the first one
// there. A note that never gets a note-off lasts `unterminatedSeconds`.
inline std::vector<Note> pairedNotes(const MidiFile& file, double unterminatedSeconds = 4.0) {
    std::vector<Note> out;
    struct Open {
        bool active = false;
        double start = 0.0;
        int velocity = 0;
    };
    std::vector<Open> open(16 * 128);
    for (const NoteEvent& e : noteEvents(file)) {
        Open& o = open[static_cast<std::size_t>(e.channel * 128 + (e.key & 127))];
        if (o.active)
            out.push_back({o.start, e.seconds, e.key & 127, e.channel, o.velocity});
        if (e.on)
            o = {true, e.seconds, std::max(1, static_cast<int>(e.velocity * 127.0f + 0.5f))};
        else
            o.active = false;
    }
    for (std::size_t i = 0; i < open.size(); ++i)
        if (open[i].active)
            out.push_back({open[i].start, open[i].start + unterminatedSeconds, static_cast<int>(i % 128),
                           static_cast<int>(i / 128), open[i].velocity});
    std::stable_sort(out.begin(), out.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
    return out;
}

} // namespace spux::inline v1::midi
