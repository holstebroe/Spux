// MIDI files: the writer, the reader and the note helpers, including byte-for-byte agreement with the
// serializers they were merged from (Acidus MidiExport, Paulascape midi_export) and the cases of the
// Lumeoid and Plumeflow readers.
#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "spux/midi/MidiFile.hpp"
#include "spux/midi/MidiNotes.hpp"
#include "spux/midi/MidiWriter.hpp"

using namespace spux::midi;

namespace {

// The serialization half of Acidus's patternMidiFile (sequencer/MidiExport.cpp), as it was when the code
// moved, taking the already rendered notes.
struct AcNote {
    int start, end, key, velocity;
};
struct AcPressure {
    int tick, key, value;
};
std::string acidusFile(const std::vector<AcNote>& notes, const std::vector<AcPressure>& pressures,
                       const std::string& name, int steps) {
    constexpr int kMidiPpq = 96, kMidiTicksPerStep = kMidiPpq / 4;
    auto putVarLen = [](std::string& out, std::uint32_t v) {
        std::uint8_t bytes[5];
        int n = 0;
        bytes[n++] = v & 0x7F;
        while ((v >>= 7) != 0)
            bytes[n++] = static_cast<std::uint8_t>(0x80 | (v & 0x7F));
        while (n > 0)
            out += static_cast<char>(bytes[--n]);
    };
    auto putBE = [](std::string& out, std::uint32_t v, int bytes) {
        for (int i = bytes - 1; i >= 0; --i)
            out += static_cast<char>((v >> (8 * i)) & 0xFF);
    };
    struct Ev {
        int tick;
        int order;
        std::uint8_t status;
        int key;
        int value;
    };
    std::vector<Ev> events;
    for (const AcNote& n : notes) {
        events.push_back({n.start, 1, 0x90, n.key, n.velocity});
        events.push_back({n.end, 0, 0x80, n.key, 64});
    }
    for (const AcPressure& p : pressures)
        events.push_back({p.tick, 2, 0xA0, p.key, p.value});
    std::stable_sort(events.begin(), events.end(),
                     [](const Ev& a, const Ev& b) { return a.tick != b.tick ? a.tick < b.tick : a.order < b.order; });
    std::string track;
    putVarLen(track, 0);
    track += "\xFF\x03";
    putVarLen(track, static_cast<std::uint32_t>(name.size()));
    track += name;
    putVarLen(track, 0);
    track += std::string("\xFF\x58\x04\x04\x02\x18\x08", 7);
    int tick = 0;
    for (const Ev& e : events) {
        putVarLen(track, static_cast<std::uint32_t>(e.tick - tick));
        tick = e.tick;
        track += static_cast<char>(e.status);
        track += static_cast<char>(e.key);
        track += static_cast<char>(e.value);
    }
    putVarLen(track, static_cast<std::uint32_t>(std::max(0, steps * kMidiTicksPerStep - tick)));
    track += std::string("\xFF\x2F\x00", 3);
    std::string file("MThd", 4);
    putBE(file, 6, 4);
    putBE(file, 0, 2);
    putBE(file, 1, 2);
    putBE(file, kMidiPpq, 2);
    file += "MTrk";
    putBE(file, static_cast<std::uint32_t>(track.size()), 4);
    file += track;
    return file;
}

std::vector<std::uint8_t> smf(std::initializer_list<std::uint8_t> track, int division = 480) {
    std::vector<std::uint8_t> v = {'M',
                                   'T',
                                   'h',
                                   'd',
                                   0,
                                   0,
                                   0,
                                   6,
                                   0,
                                   0,
                                   0,
                                   1,
                                   static_cast<std::uint8_t>(division >> 8),
                                   static_cast<std::uint8_t>(division)};
    v.insert(v.end(), {'M', 'T', 'r', 'k', 0, 0, 0, static_cast<std::uint8_t>(track.size())});
    v.insert(v.end(), track.begin(), track.end());
    return v;
}
} // namespace

TEST_CASE("the writer produces the bytes Acidus's serializer produced") {
    // A 16-step pattern: a slide (overlap), two notes ending where another starts, pressure on a held note.
    const std::vector<AcNote> notes = {
        {0, 26, 36, 100}, {24, 48, 38, 90}, {48, 72, 38, 127}, {96, 120, 41, 1}, {200, 384, 60, 64}};
    const std::vector<AcPressure> pressures = {{30, 38, 90}, {60, 38, 127}, {96, 41, 5}};
    const std::string expected = acidusFile(notes, pressures, "Pattern A1", 16);

    FileWriter file(0, 96);
    TrackWriter& t = file.addTrack();
    t.name("Pattern A1");
    t.timeSignature(0, 4, 2);
    // added in a different order than the ticks: the writer sorts
    for (const AcPressure& p : pressures)
        t.pressure(static_cast<std::uint32_t>(p.tick), 0, p.key, p.value);
    for (const AcNote& n : notes) {
        t.noteOn(static_cast<std::uint32_t>(n.start), 0, n.key, n.velocity);
        t.noteOff(static_cast<std::uint32_t>(n.end), 0, n.key, 64);
    }
    t.setEndTick(16 * 24);
    const std::vector<std::uint8_t> got = file.bytes();
    CHECK(std::string(got.begin(), got.end()) == expected);
}

TEST_CASE("at one tick: note-offs, then meta and controllers, then note-ons, then pressure") {
    FileWriter file(1, 96);
    TrackWriter& t = file.addTrack();
    t.noteOn(10, 0, 60, 100);
    t.pressure(10, 0, 60, 7);
    t.controller(10, 0, 1, 64);
    t.noteOff(10, 0, 62, 0);
    t.pitchBend(10, 0, 8192);
    t.program(10, 0, 5);
    MidiFile m;
    REQUIRE(parseMidiFile(file.bytes(), m));
    std::vector<int> kinds;
    for (const MidiEvent& e : m.events)
        if (e.isChannel())
            kinds.push_back(e.kind());
    // off(0) and bend(0) first in the order added, then controller and program (1), note-on (2), pressure (3)
    CHECK(kinds == std::vector<int>{0x80, 0xE0, 0xB0, 0xC0, 0x90, 0xA0});
}

TEST_CASE("a multi-track file with a tempo map reads back") {
    FileWriter file(1, 96);
    TrackWriter& conductor = file.addTrack();
    conductor.name("song");
    conductor.tempo(0, 500000);
    conductor.tempo(96, 1000000);
    TrackWriter& ch = file.addTrack();
    ch.name("MOD channel 1");
    ch.program(0, 2, 7);
    ch.noteOn(0, 2, 60, 100);
    ch.noteOff(96, 2, 60, 0);
    ch.noteOn(96, 2, 62, 90);
    ch.noteOff(192, 2, 62, 0);
    ch.setEndTick(400);
    MidiFile m;
    std::string error;
    REQUIRE(parseMidiFile(file.bytes(), m, &error));
    CHECK(m.format == 1);
    CHECK(m.division == 96);
    CHECK(m.trackCount == 2);
    CHECK(m.trackEndTicks == std::vector<std::uint32_t>{96, 400});

    const TempoMap tempo(m);
    CHECK(tempo.seconds(0) == 0.0);
    CHECK(tempo.seconds(96) == doctest::Approx(0.5));
    CHECK(tempo.seconds(192) == doctest::Approx(1.5)); // the second quarter note lasts a second

    const std::vector<NoteEvent> events = noteEvents(m);
    REQUIRE(events.size() == 4);
    CHECK(events[0].on);
    CHECK(events[0].channel == 2);
    CHECK(events[0].velocity == doctest::Approx(100.0 / 127.0));
    CHECK_FALSE(events[1].on); // off before on at tick 96
    CHECK(events[2].on);
    CHECK(events[2].seconds == doctest::Approx(0.5));
    CHECK(events[3].seconds == doctest::Approx(1.5));
}

TEST_CASE("Lumeoid's cases: tempo changes land notes at the right time") {
    // 120 bpm, note on at 0, off after a quarter (0.5 s), then 60 bpm and another note a quarter later (+1 s).
    MidiFile m;
    REQUIRE(parseMidiFile(smf({0x00, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20, 0x00, 0x90, 60,   100,  0x83, 0x60, 0x80,
                               60,   0,    0x00, 0xff, 0x51, 0x03, 0x0f, 0x42, 0x40, 0x83, 0x60, 0x90, 62,   127}),
                          m));
    const auto notes = noteEvents(m);
    REQUIRE(notes.size() == 3);
    CHECK(notes[0].on);
    CHECK(notes[0].seconds == doctest::Approx(0.0));
    CHECK(notes[0].velocity == doctest::Approx(100.0 / 127.0));
    CHECK_FALSE(notes[1].on);
    CHECK(notes[1].seconds == doctest::Approx(0.5));
    CHECK(notes[2].key == 62);
    CHECK(notes[2].seconds == doctest::Approx(1.5));
}

TEST_CASE("running status is expanded, a zero-velocity note-on is a note-off") {
    MidiFile m;
    REQUIRE(parseMidiFile(smf({0x00, 0x90, 60, 100, 0x10, 62, 90, 0x10, 60, 0, 0x00, 0xC1, 9, 0x00, 0x62}), m));
    // the last event reuses the running status of the program change (0xC1): one data byte
    REQUIRE(m.events.size() == 5);
    CHECK(m.events[1].data == std::vector<std::uint8_t>{0x90, 62, 90});
    CHECK(m.events[2].isNoteOff());
    CHECK_FALSE(m.events[2].isNoteOn());
    CHECK(m.events[3].data == std::vector<std::uint8_t>{0xC1, 9});
    CHECK(m.events[4].data == std::vector<std::uint8_t>{0xC1, 0x62});
}

TEST_CASE("meta events cancel running status, sysex is kept") {
    MidiFile m;
    CHECK_FALSE(parseMidiFile(smf({0x00, 0x90, 60, 100, 0x00, 0xFF, 0x01, 0x01, 'x', 0x00, 61, 100}),
                              m)); // data without status
    REQUIRE(parseMidiFile(smf({0x00, 0xF0, 0x03, 1, 2, 0xF7, 0x00, 0xFF, 0x2F, 0x00}), m));
    CHECK(m.events[0].isSysEx());
    CHECK(m.events[0].data == std::vector<std::uint8_t>{0xF0, 1, 2, 0xF7});
    CHECK(m.events[1].metaType() == 0x2F);
}

TEST_CASE("Plumeflow's reader: paired notes, with a tail for the unterminated") {
    FileWriter file(0, 480);
    TrackWriter& t = file.addTrack();
    t.noteOn(0, 0, 60, 100);
    t.noteOff(480, 0, 60, 0); // 0.5 s at the default 120 bpm
    t.noteOn(480, 0, 62, 80); // starts where the last note ended
    t.noteOn(960, 0, 62, 70); // retriggered while sounding: ends the first
    t.noteOn(0, 3, 40, 50);   // another channel, never released
    t.noteOff(1440, 0, 62, 0);
    MidiFile m;
    REQUIRE(parseMidiFile(file.bytes(), m));
    const auto notes = pairedNotes(m, 4.0);
    REQUIRE(notes.size() == 4);
    CHECK(notes[0].key == 60);
    CHECK(notes[0].start == doctest::Approx(0.0));
    CHECK(notes[0].end == doctest::Approx(0.5));
    CHECK(notes[0].velocity == 100);
    const auto it = std::find_if(notes.begin(), notes.end(), [](const Note& n) { return n.key == 40; });
    REQUIRE(it != notes.end());
    CHECK(it->channel == 3);
    CHECK(it->end - it->start == doctest::Approx(4.0));
    const auto firstRetrigger =
        std::find_if(notes.begin(), notes.end(), [](const Note& n) { return n.key == 62 && n.velocity == 80; });
    REQUIRE(firstRetrigger != notes.end());
    CHECK(firstRetrigger->start == doctest::Approx(0.5));
    CHECK(firstRetrigger->end == doctest::Approx(1.0));
}

TEST_CASE("unknown chunks are skipped, SMPTE and garbage are rejected") {
    FileWriter file(1, 96);
    file.addTrack().noteOn(0, 0, 60, 100);
    auto bytes = file.bytes();
    std::vector<std::uint8_t> withJunk(bytes.begin(), bytes.begin() + 14);
    withJunk.insert(withJunk.end(), {'X', 'F', 'I', 'H', 0, 0, 0, 2, 9, 9});
    withJunk.insert(withJunk.end(), bytes.begin() + 14, bytes.end());
    MidiFile m;
    CHECK(parseMidiFile(withJunk, m));
    CHECK(m.events.size() == 2); // the note-on and the writer's end-of-track
    std::string error;
    auto smpte = bytes;
    smpte[12] = 0xE7;
    CHECK_FALSE(parseMidiFile(smpte, m, &error));
    CHECK(error.find("SMPTE") != std::string::npos);
    CHECK_FALSE(parseMidiFile(std::vector<std::uint8_t>{}, m, &error));
    CHECK_FALSE(parseMidiFile(std::vector<std::uint8_t>(40, 'M'), m, &error));
    auto fiveTracks = bytes;
    fiveTracks[11] = 5;
    CHECK_FALSE(parseMidiFile(fiveTracks, m, &error));
}

TEST_CASE("truncated and corrupted files never crash") {
    FileWriter file(1, 96);
    TrackWriter& t = file.addTrack();
    t.name("x");
    t.tempo(0, 500000);
    for (int i = 0; i < 20; ++i) {
        t.noteOn(static_cast<std::uint32_t>(i * 24), 1, 40 + i, 100);
        t.noteOff(static_cast<std::uint32_t>(i * 24 + 20), 1, 40 + i, 0);
    }
    const auto bytes = file.bytes();
    MidiFile m;
    for (std::size_t cut = 0; cut < bytes.size(); ++cut)
        parseMidiFile(bytes.data(), cut, m); // either result, no crash
    std::uint32_t s = 99;
    for (int round = 0; round < 2000; ++round) {
        auto damaged = bytes;
        for (int k = 0; k < 3; ++k) {
            s = s * 1664525u + 1013904223u;
            damaged[(s >> 8) % damaged.size()] = static_cast<std::uint8_t>(s >> 24);
        }
        if (parseMidiFile(damaged, m))
            noteEvents(m); // a file that parses can be converted
    }
    CHECK(true);
}

TEST_CASE("variable-length quantities: the largest and a too long one") {
    std::vector<std::uint8_t> v;
    putVarLen(v, 0x0FFFFFFF);
    CHECK(v == std::vector<std::uint8_t>{0xFF, 0xFF, 0xFF, 0x7F});
    v.clear();
    putVarLen(v, 0x80000000u); // clamped
    CHECK(v.size() == 4);
    v.clear();
    putVarLen(v, 0);
    CHECK(v == std::vector<std::uint8_t>{0});
    MidiFile m;
    CHECK_FALSE(parseMidiFile(smf({0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x90, 60, 100}), m)); // five bytes
}

TEST_CASE("files: write and read") {
    const auto dir = std::filesystem::temp_directory_path() / "spux_midi_test";
    std::filesystem::create_directories(dir);
    FileWriter file(0, 96);
    file.addTrack().noteOn(0, 0, 60, 100);
    const std::string path = (dir / "a.mid").string();
    REQUIRE(file.write(path));
    MidiFile m;
    REQUIRE(readMidiFile(path, m));
    CHECK(m.events.size() == 2); // the note and the end of track
    CHECK_FALSE(readMidiFile((dir / "none.mid").string(), m));
    CHECK_FALSE(file.write((dir / "no" / "such" / "dir.mid").string()));
    std::filesystem::remove_all(dir);
}
