// The chunked state container (decisions P3, S7, P9), from Lumeoid's `LMN1` (state/container).
//
//   magic    4 bytes, chosen by the plugin ("LMN1", "PLF1", ...)
//   version  uint32, the plugin's state version (the plugin decides what a version means, P9)
//   chunks   id (4 bytes), length (uint32), CRC-32 of the payload (uint32), payload
// Little-endian. Host state, user presets and factory presets can all be this one container (P4).
//
// Reading never trusts a length or a count: a chunk whose length runs past the end ends the read (and
// counts as damaged), a chunk whose CRC does not match is skipped and counted as damaged, a chunk the
// plugin does not know is skipped and counted as unknown, so a later version can add chunks without
// breaking older readers. The plugin's chunk handler decides what is known and what is damaged.
//
// Migration: the version read is the plugin's input to migrate() (one step per released version), so a
// later converter can restore old behaviour or rescue old data. A state from before the container
// (Tearwash's text state) has no magic: hasMagic() is false and the plugin runs its own legacy reader.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

#include "spux/state/Bytes.hpp"

namespace spux::inline v1::state {

inline constexpr std::size_t kHeaderBytes = 8;           // magic + version
inline constexpr std::size_t kChunkHeaderBytes = 12;     // id + length + CRC
inline constexpr std::size_t kMaxChunkBytes = 64u << 20; // anything larger is treated as damaged

class ContainerWriter {
public:
    ContainerWriter(FourCC magic, std::uint32_t version) {
        w_.fourcc(magic);
        w_.u32(version);
    }
    void chunk(FourCC id, std::span<const std::uint8_t> payload) {
        w_.fourcc(id);
        w_.u32(static_cast<std::uint32_t>(payload.size()));
        w_.u32(crc32(payload));
        w_.bytes(payload);
    }
    void chunk(FourCC id, const ByteWriter& payload) { chunk(id, std::span<const std::uint8_t>(payload.data())); }
    [[nodiscard]] std::vector<std::uint8_t> take() { return w_.take(); }

private:
    ByteWriter w_;
};

enum class ReadStatus : std::uint8_t { Ok, TooShort, BadMagic };

// What the chunk handler says about a chunk.
enum class ChunkResult : std::uint8_t { Read, Unknown, Damaged };

struct ReadReport {
    ReadStatus status = ReadStatus::TooShort;
    std::uint32_t version = 0;
    int chunks = 0;        // chunks the handler read
    int unknownChunks = 0; // skipped: the handler does not know them
    int damagedChunks = 0; // skipped: bad length, bad CRC or content the handler rejected
    [[nodiscard]] bool ok() const { return status == ReadStatus::Ok; }
};

[[nodiscard]] inline bool hasMagic(std::span<const std::uint8_t> data, FourCC magic) {
    return data.size() >= 4 && std::memcmp(data.data(), magic.c.data(), 4) == 0;
}

// Reads a container. `onChunk(FourCC id, std::span<const std::uint8_t> payload, std::uint32_t version)`
// is called for every chunk with a good CRC, in file order, and returns a ChunkResult. Not real-time.
template <class OnChunk>
ReadReport readContainer(std::span<const std::uint8_t> data, FourCC magic, OnChunk&& onChunk,
                         std::size_t maxChunkBytes = kMaxChunkBytes) {
    ReadReport r;
    if (data.size() < kHeaderBytes)
        return r;
    if (!hasMagic(data, magic)) {
        r.status = ReadStatus::BadMagic;
        return r;
    }
    ByteReader head(data.subspan(4, 4));
    r.version = head.u32();
    r.status = ReadStatus::Ok;
    std::size_t pos = kHeaderBytes;
    while (data.size() - pos >= kChunkHeaderBytes) {
        const FourCC id = FourCC::fromBytes(data.data() + pos);
        ByteReader h(data.subspan(pos + 4, 8));
        const std::size_t length = h.u32();
        const std::uint32_t crc = h.u32();
        pos += kChunkHeaderBytes;
        if (length > maxChunkBytes || length > data.size() - pos) {
            ++r.damagedChunks; // the length is not trustworthy: nothing after it can be located
            break;
        }
        const auto payload = data.subspan(pos, length);
        pos += length;
        if (crc32(payload) != crc) {
            ++r.damagedChunks;
            continue;
        }
        switch (onChunk(id, payload, r.version)) {
        case ChunkResult::Read:
            ++r.chunks;
            break;
        case ChunkResult::Unknown:
            ++r.unknownChunks;
            break;
        case ChunkResult::Damaged:
            ++r.damagedChunks;
            break;
        }
    }
    return r;
}

// One migration step: turns data of version `from` into version `from + 1`.
template <class Data> struct Migration {
    std::uint32_t from = 0;
    void (*apply)(Data&) = nullptr;
};

// Applies the steps from `fromVersion` up, in order; returns the version reached. A state newer than
// every step is left as it is (read as far as its chunks are understood).
template <class Data>
std::uint32_t migrate(Data& data, std::uint32_t fromVersion, std::span<const Migration<Data>> steps) {
    std::uint32_t v = fromVersion;
    for (bool progressed = true; progressed;) {
        progressed = false;
        for (const Migration<Data>& m : steps)
            if (m.from == v && m.apply != nullptr) {
                m.apply(data);
                ++v;
                progressed = true;
            }
    }
    return v;
}

// The parameter chunk "PARM" (Lumeoid's layout): uint32 count, then count pairs of uint32 id and
// float64 value. Values are the plugin's choice of domain (plain values recommended: they survive a
// change of host mapping). Readers ignore ids they do not know and keep defaults for missing ones.
inline constexpr FourCC kParamChunk{"PARM"};

// `value(i)` returns the pair {id, value} of entry i < count.
template <class Value> ByteWriter paramPayload(std::size_t count, Value&& value) {
    ByteWriter w;
    w.u32(static_cast<std::uint32_t>(count));
    for (std::size_t i = 0; i < count; ++i) {
        const std::pair<std::uint32_t, double> e = value(i);
        w.u32(e.first);
        w.f64(e.second);
    }
    return w;
}

// Calls onValue(id, value) for every entry; false (nothing called) when the payload is damaged.
template <class OnValue> bool readParamPayload(std::span<const std::uint8_t> payload, OnValue&& onValue) {
    ByteReader r(payload);
    const std::size_t count = r.u32();
    if (!r.ok() || count > r.remaining() / 12)
        return false;
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t id = r.u32();
        const double v = r.f64();
        onValue(id, v);
    }
    return true;
}

} // namespace spux::inline v1::state
