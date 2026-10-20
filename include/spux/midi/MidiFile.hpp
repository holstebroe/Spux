// Standard MIDI File reader (formats 0, 1 and 2, PPQ time division). It keeps every event, in file order
// track by track, with its absolute tick; helpers that turn those into timed notes are in MidiNotes.hpp.
//
// Merged from Paulascape (midi_file_reader.hpp: raw events), Lumeoid (tools/midi_file.cpp: bounds checks,
// tempo map) and Plumeflow (tools/MidiFile.hpp: paired notes). It never throws and never asserts: a
// malformed file returns false with a message, and a truncated one is cut off at the damage.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace spux::inline v1::midi {

struct MidiEvent {
    std::uint32_t tick = 0;
    int track = 0;
    // A channel message: {status, data1[, data2]} with the status always present (running status is
    // expanded). A meta event: {0xFF, type, payload...}. System exclusive: {0xF0 or 0xF7, payload...}.
    std::vector<std::uint8_t> data;

    bool isMeta() const { return !data.empty() && data[0] == 0xFF; }
    bool isSysEx() const { return !data.empty() && (data[0] == 0xF0 || data[0] == 0xF7); }
    bool isChannel() const { return !data.empty() && data[0] >= 0x80 && data[0] < 0xF0; }
    int metaType() const { return isMeta() && data.size() >= 2 ? data[1] : -1; }
    // The kind of a channel message (status & 0xF0: 0x80 off, 0x90 on, 0xA0 pressure, 0xB0 controller,
    // 0xC0 program, 0xD0 channel pressure, 0xE0 bend), or -1.
    int kind() const { return isChannel() ? data[0] & 0xF0 : -1; }
    int channel() const { return isChannel() ? data[0] & 0x0F : -1; }
    // A note-on with velocity 0 is a note-off.
    bool isNoteOn() const { return kind() == 0x90 && data.size() >= 3 && data[2] > 0; }
    bool isNoteOff() const { return kind() == 0x80 || (kind() == 0x90 && data.size() >= 3 && data[2] == 0); }
};

struct MidiFile {
    int format = 0;
    int division = 480; // ticks per quarter note
    int trackCount = 0;
    std::vector<MidiEvent> events;            // file order: track by track
    std::vector<std::uint32_t> trackEndTicks; // the tick of each track's end, including the end-of-track delta
};

namespace detail {

struct Cursor {
    const std::uint8_t* p;
    const std::uint8_t* end;
    bool ok = true;

    std::size_t left() const { return static_cast<std::size_t>(end - p); }
    std::uint32_t u8() {
        if (p >= end) {
            ok = false;
            return 0;
        }
        return *p++;
    }
    std::uint32_t be(int n) {
        std::uint32_t v = 0;
        for (int i = 0; i < n; ++i)
            v = (v << 8) | u8();
        return v;
    }
    // Variable-length quantity, at most four bytes.
    std::uint32_t vlq() {
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            const std::uint32_t b = u8();
            v = (v << 7) | (b & 0x7F);
            if (!(b & 0x80) || !ok)
                return v;
        }
        ok = false; // a fifth byte: not a valid quantity
        return v;
    }
};

inline bool fail(std::string* error, const char* message) {
    if (error)
        *error = message;
    return false;
}

} // namespace detail

inline bool parseMidiFile(const std::uint8_t* bytes, std::size_t size, MidiFile& out, std::string* error = nullptr) {
    using detail::fail;
    out = MidiFile{};
    detail::Cursor r{bytes, bytes + size};
    if (size < 14 || r.be(4) != 0x4D546864u /* MThd */)
        return fail(error, "not a MIDI file");
    const std::uint32_t headerLen = r.be(4);
    if (headerLen < 6 || headerLen > r.left())
        return fail(error, "bad MIDI header");
    out.format = static_cast<int>(r.be(2));
    const int tracks = static_cast<int>(r.be(2));
    const std::uint32_t division = r.be(2);
    r.p += headerLen - 6;
    if (division & 0x8000u)
        return fail(error, "SMPTE time division is not supported");
    if (division == 0)
        return fail(error, "time division is zero");
    out.division = static_cast<int>(division);

    for (int t = 0; t < tracks;) {
        if (r.left() < 8)
            return fail(error, "missing track chunk");
        const std::uint32_t id = r.be(4), len = r.be(4);
        if (len > r.left())
            return fail(error, "truncated track");
        if (id != 0x4D54726Bu /* MTrk */) { // unknown chunk: skipped, as the specification asks
            r.p += len;
            continue;
        }
        detail::Cursor tr{r.p, r.p + len};
        r.p += len;
        std::uint32_t tick = 0;
        std::uint32_t status = 0; // running status
        while (tr.p < tr.end) {
            tick += tr.vlq();
            if (!tr.ok || tr.p >= tr.end)
                return fail(error, "truncated event");
            std::uint32_t b = tr.u8();
            MidiEvent e;
            e.tick = tick;
            e.track = t;
            if (b & 0x80) {
                status = b;
            } else {
                if (status == 0)
                    return fail(error, "data byte without a status");
                --tr.p; // running status: this byte is the first data byte
                b = status;
            }
            if (b == 0xFF) {
                status = 0;
                const std::uint32_t type = tr.u8(), n = tr.vlq();
                if (!tr.ok || n > tr.left())
                    return fail(error, "truncated meta event");
                e.data = {0xFF, static_cast<std::uint8_t>(type)};
                e.data.insert(e.data.end(), tr.p, tr.p + n);
                tr.p += n;
            } else if (b == 0xF0 || b == 0xF7) {
                status = 0;
                const std::uint32_t n = tr.vlq();
                if (!tr.ok || n > tr.left())
                    return fail(error, "truncated system exclusive event");
                e.data = {static_cast<std::uint8_t>(b)};
                e.data.insert(e.data.end(), tr.p, tr.p + n);
                tr.p += n;
            } else if (b >= 0xF1) {
                return fail(error, "unsupported system message in a track");
            } else {
                const std::uint32_t kind = b & 0xF0;
                const int args = (kind == 0xC0 || kind == 0xD0) ? 1 : 2;
                e.data.push_back(static_cast<std::uint8_t>(b));
                for (int a = 0; a < args; ++a)
                    e.data.push_back(static_cast<std::uint8_t>(tr.u8()));
                if (!tr.ok)
                    return fail(error, "truncated event");
            }
            out.events.push_back(std::move(e));
        }
        out.trackEndTicks.push_back(tick);
        ++t;
    }
    out.trackCount = tracks;
    return true;
}

inline bool parseMidiFile(const std::vector<std::uint8_t>& bytes, MidiFile& out, std::string* error = nullptr) {
    return parseMidiFile(bytes.data(), bytes.size(), out, error);
}

inline bool readMidiFile(const std::string& path, MidiFile& out, std::string* error = nullptr) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return detail::fail(error, "cannot open the MIDI file");
    std::vector<std::uint8_t> bytes;
    std::uint8_t buf[1 << 14];
    for (std::size_t n; (n = std::fread(buf, 1, sizeof(buf), f)) > 0;)
        bytes.insert(bytes.end(), buf, buf + n);
    std::fclose(f);
    return parseMidiFile(bytes, out, error);
}

} // namespace spux::inline v1::midi
