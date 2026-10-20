// spux_state: CRC-32, byte reading and writing, the container (magic, version, CRC, unknown and damaged
// chunks), the parameter chunk, the migration hook, and a fixed byte image so the format cannot drift.
#include <doctest.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "spux/state/Container.hpp"

using namespace spux::state;

namespace {

std::span<const std::uint8_t> bytesOf(const char* s) {
    return {reinterpret_cast<const std::uint8_t*>(s), std::strlen(s)};
}

struct Read {
    std::map<std::string, std::vector<std::uint8_t>> chunks;
    ReadReport report;
};

// Knows NAME and PARM; NAME must not be empty.
Read readTest(std::span<const std::uint8_t> data) {
    Read out;
    out.report =
        readContainer(data, FourCC("TST1"), [&](FourCC id, std::span<const std::uint8_t> payload, std::uint32_t) {
            if (id == FourCC("NAME")) {
                if (payload.empty())
                    return ChunkResult::Damaged;
            } else if (!(id == kParamChunk)) {
                return ChunkResult::Unknown;
            }
            out.chunks[std::string(id.view())].assign(payload.begin(), payload.end());
            return ChunkResult::Read;
        });
    return out;
}

std::vector<std::uint8_t> sample() {
    ContainerWriter w(FourCC("TST1"), 3);
    w.chunk(FourCC("NAME"), bytesOf("Init"));
    w.chunk(FourCC("NEW!"), bytesOf("from a later version"));
    w.chunk(kParamChunk, paramPayload(2, [](std::size_t i) {
                return std::pair<std::uint32_t, double>{i == 0 ? 7u : 1001u, i == 0 ? 0.25 : -12.5};
            }));
    return w.take();
}

} // namespace

TEST_CASE("CRC-32 is the IEEE one") {
    static constexpr std::uint8_t kCheck[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    static_assert(crc32(kCheck, 9) == 0xCBF43926u);
    CHECK(crc32(bytesOf("")) == 0u);
}

TEST_CASE("bytes: little-endian, and reads stop at the end") {
    ByteWriter w;
    w.u8(1);
    w.u16(0x0302);
    w.u32(0x07060504u);
    w.f64(-0.5);
    w.string("ab");
    const auto& d = w.data();
    CHECK(d.size() == 1 + 2 + 4 + 8 + 4 + 2);
    CHECK((d[0] == 1 && d[1] == 2 && d[2] == 3 && d[3] == 4 && d[6] == 7));
    ByteReader r(d);
    CHECK(r.u8() == 1);
    CHECK(r.u16() == 0x0302);
    CHECK(r.u32() == 0x07060504u);
    CHECK(r.f64() == -0.5);
    CHECK(r.string() == "ab");
    CHECK(r.ok());
    CHECK(r.u32() == 0);
    CHECK(!r.ok());
    ByteReader big(std::span<const std::uint8_t>(d).first(4));
    big.u8();
    CHECK(big.string(16).empty()); // the length (0x07060504 bytes) does not fit
    CHECK(!big.ok());
}

TEST_CASE("container: magic, version, chunks in order, unknown chunks skipped") {
    const auto data = sample();
    const Read r = readTest(data);
    CHECK(r.report.ok());
    CHECK(r.report.version == 3);
    CHECK(r.report.chunks == 2);
    CHECK(r.report.unknownChunks == 1);
    CHECK(r.report.damagedChunks == 0);
    CHECK(std::string(r.chunks.at("NAME").begin(), r.chunks.at("NAME").end()) == "Init");

    std::map<std::uint32_t, double> values;
    CHECK(readParamPayload(r.chunks.at("PARM"), [&](std::uint32_t id, double v) { values[id] = v; }));
    CHECK(values.size() == 2);
    CHECK(values[7] == 0.25);
    CHECK(values[1001] == -12.5);
}

TEST_CASE("container: the byte image is fixed (Lumeoid's LMN1 layout)") {
    const auto data = sample();
    const std::uint8_t head[] = {'T', 'S', 'T', '1', 3, 0, 0, 0, 'N', 'A', 'M', 'E', 4, 0, 0, 0};
    CHECK(std::memcmp(data.data(), head, sizeof head) == 0);
    const std::uint32_t crc = crc32(bytesOf("Init"));
    CHECK(std::memcmp(data.data() + 16, &crc, 4) == 0); // little-endian hosts
    CHECK(std::memcmp(data.data() + 20, "Init", 4) == 0);
    CHECK(data.size() == 8 + (12 + 4) + (12 + 20) + (12 + 4 + 2 * 12));
}

TEST_CASE("container: bad magic, too short, damaged chunks") {
    auto data = sample();
    CHECK(readTest(std::span<const std::uint8_t>(data).first(7)).report.status == ReadStatus::TooShort);
    auto other = data;
    other[3] = '2';
    CHECK(readTest(other).report.status == ReadStatus::BadMagic);
    CHECK(!hasMagic(other, FourCC("TST1")));
    CHECK(hasMagic(data, FourCC("TST1")));

    auto flipped = data;
    flipped[20] ^= 0x20; // NAME payload: the CRC no longer matches
    const Read f = readTest(flipped);
    CHECK(f.report.ok());
    CHECK(f.report.damagedChunks == 1);
    CHECK(f.chunks.count("NAME") == 0);
    CHECK(f.chunks.count("PARM") == 1); // the rest still loads

    auto truncated = data;
    truncated.resize(truncated.size() - 5); // PARM runs past the end
    const Read t = readTest(truncated);
    CHECK(t.report.damagedChunks == 1);
    CHECK(t.chunks.count("NAME") == 1);
    CHECK(t.chunks.count("PARM") == 0);

    ContainerWriter w(FourCC("TST1"), 1);
    w.chunk(FourCC("NAME"), std::span<const std::uint8_t>{}); // the handler rejects it
    CHECK(readTest(w.take()).report.damagedChunks == 1);
}

TEST_CASE("parameter payload: counts are not trusted") {
    ByteWriter w;
    w.u32(1000); // claims 1000 entries
    w.u32(1);
    w.f64(1.0);
    int calls = 0;
    CHECK(!readParamPayload(w.data(), [&](std::uint32_t, double) { ++calls; }));
    CHECK(calls == 0);
    CHECK(!readParamPayload({}, [&](std::uint32_t, double) { ++calls; }));
}

TEST_CASE("migration: one step per version, from the version read") {
    struct Data {
        std::string log;
    };
    static constexpr Migration<Data> steps[] = {
        {2, [](Data& d) { d.log += "2>3 "; }},
        {1, [](Data& d) { d.log += "1>2 "; }},
        {3, [](Data& d) { d.log += "3>4 "; }},
    };
    Data a;
    CHECK(migrate<Data>(a, 1, steps) == 4);
    CHECK(a.log == "1>2 2>3 3>4 ");
    Data b;
    CHECK(migrate<Data>(b, 3, steps) == 4);
    CHECK(b.log == "3>4 ");
    Data c;
    CHECK(migrate<Data>(c, 9, steps) == 9); // newer than every step: as it is
    CHECK(c.log.empty());
}
