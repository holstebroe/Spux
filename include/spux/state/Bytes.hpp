// Little-endian byte writing and bounds-checked reading, CRC-32 and four-character codes for the state
// container (from Lumeoid `state/container`). Nothing here runs on the audio thread.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "spux/Version.hpp"

namespace spux::inline v1::state {

namespace detail {
constexpr std::array<std::uint32_t, 256> makeCrcTable() {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        t[i] = c;
    }
    return t;
}
inline constexpr auto kCrcTable = makeCrcTable();
} // namespace detail

// CRC-32 (IEEE 802.3, reflected, as zlib and PNG): crc32("123456789") == 0xCBF43926.
constexpr std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i)
        c = detail::kCrcTable[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
inline std::uint32_t crc32(std::span<const std::uint8_t> data) { return crc32(data.data(), data.size()); }

// A four-character code: a container magic ("LMN1") or a chunk id ("PARM"). Written as its four bytes.
struct FourCC {
    std::array<char, 4> c{};
    constexpr FourCC() = default;
    constexpr FourCC(const char (&s)[5]) : c{s[0], s[1], s[2], s[3]} {}
    [[nodiscard]] static FourCC fromBytes(const std::uint8_t* p) {
        FourCC f;
        std::memcpy(f.c.data(), p, 4);
        return f;
    }
    [[nodiscard]] constexpr std::string_view view() const { return {c.data(), 4}; }
    constexpr bool operator==(const FourCC&) const = default;
};

class ByteWriter {
public:
    void u8(std::uint8_t x) { out_.push_back(x); }
    void u16(std::uint16_t x) { put(x, 2); }
    void u32(std::uint32_t x) { put(x, 4); }
    void u64(std::uint64_t x) { put(x, 8); }
    void f32(float f) {
        std::uint32_t u;
        std::memcpy(&u, &f, 4);
        u32(u);
    }
    void f64(double d) {
        std::uint64_t u;
        std::memcpy(&u, &d, 8);
        u64(u);
    }
    void fourcc(FourCC f) { out_.insert(out_.end(), f.c.begin(), f.c.end()); }
    void bytes(std::span<const std::uint8_t> b) { out_.insert(out_.end(), b.begin(), b.end()); }
    void bytes(std::string_view s) { out_.insert(out_.end(), s.begin(), s.end()); }
    // A uint32 length, then the bytes (no terminator).
    void string(std::string_view s) {
        u32(static_cast<std::uint32_t>(s.size()));
        bytes(s);
    }

    [[nodiscard]] std::size_t size() const { return out_.size(); }
    [[nodiscard]] const std::vector<std::uint8_t>& data() const { return out_; }
    [[nodiscard]] std::vector<std::uint8_t> take() { return std::move(out_); }

private:
    void put(std::uint64_t x, int n) {
        for (int i = 0; i < n; ++i)
            out_.push_back(static_cast<std::uint8_t>(x >> (8 * i)));
    }
    std::vector<std::uint8_t> out_;
};

// Reads never go past the end: a read that does not fit returns zero and clears ok(); later reads
// return zero too. Check ok() once after reading a record.
class ByteReader {
public:
    ByteReader() = default;
    explicit ByteReader(std::span<const std::uint8_t> data) : data_(data) {}

    std::uint8_t u8() { return static_cast<std::uint8_t>(get(1)); }
    std::uint16_t u16() { return static_cast<std::uint16_t>(get(2)); }
    std::uint32_t u32() { return static_cast<std::uint32_t>(get(4)); }
    std::uint64_t u64() { return get(8); }
    float f32() {
        const std::uint32_t u = u32();
        float f;
        std::memcpy(&f, &u, 4);
        return f;
    }
    double f64() {
        const std::uint64_t u = u64();
        double d;
        std::memcpy(&d, &u, 8);
        return d;
    }
    // `n` bytes, or an empty span (and !ok()) when fewer remain.
    std::span<const std::uint8_t> bytes(std::size_t n) {
        if (!ok_ || n > remaining()) {
            ok_ = false;
            return {};
        }
        const auto s = data_.subspan(pos_, n);
        pos_ += n;
        return s;
    }
    // A string written by ByteWriter::string; at most maxBytes long, longer is a damaged record.
    std::string string(std::size_t maxBytes = 1u << 20) {
        const std::size_t n = u32();
        if (n > maxBytes) {
            ok_ = false;
            return {};
        }
        const auto b = bytes(n);
        return std::string(b.begin(), b.end());
    }

    [[nodiscard]] bool ok() const { return ok_; }
    [[nodiscard]] std::size_t remaining() const { return data_.size() - pos_; }
    [[nodiscard]] std::size_t position() const { return pos_; }

private:
    std::uint64_t get(int n) {
        if (!ok_ || remaining() < static_cast<std::size_t>(n)) {
            ok_ = false;
            return 0;
        }
        std::uint64_t x = 0;
        for (int i = 0; i < n; ++i)
            x |= static_cast<std::uint64_t>(data_[pos_ + static_cast<std::size_t>(i)]) << (8 * i);
        pos_ += static_cast<std::size_t>(n);
        return x;
    }
    std::span<const std::uint8_t> data_{};
    std::size_t pos_ = 0;
    bool ok_ = true;
};

} // namespace spux::inline v1::state
