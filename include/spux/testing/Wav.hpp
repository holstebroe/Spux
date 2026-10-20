// WAV reading and writing for tests and offline tools: reference renders are stored as 32-bit float
// WAV (24-bit also reads and writes), one file per case. Interleaved float samples in memory.
//
// Writes: PCM 16 (the Kikset layout, mono or more), PCM 24 and IEEE float 32. Reads: PCM 8/16/24/32,
// float 32/64, plain or WAVE_FORMAT_EXTENSIBLE. Never throws; errors come back as a message.
// The integer formats write with the scale 2^(n-1) - 1 and read with 2^(n-1), the usual convention, so a
// round trip is off by up to 1.5 quanta; only float 32 is exact (and is what references use).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace spux::inline v1::testing {

struct WavData {
    int channels = 0;
    int sampleRate = 0;
    std::vector<float> samples; // interleaved

    std::size_t frames() const { return channels > 0 ? samples.size() / static_cast<std::size_t>(channels) : 0; }
};

enum class WavFormat { Float32, Pcm24, Pcm16 };

namespace detail {

inline void putLe(std::vector<std::uint8_t>& out, std::uint32_t v, int bytes) {
    for (int i = 0; i < bytes; ++i)
        out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}
inline void putTag(std::vector<std::uint8_t>& out, const char* tag) { out.insert(out.end(), tag, tag + 4); }

inline std::uint32_t getLe(const std::uint8_t* p, int bytes) {
    std::uint32_t v = 0;
    for (int i = 0; i < bytes; ++i)
        v |= static_cast<std::uint32_t>(p[i]) << (8 * i);
    return v;
}

inline bool fail(std::string* error, const char* message) {
    if (error)
        *error = message;
    return false;
}

} // namespace detail

// The bytes of a WAV file. Samples outside [-1, 1] are clamped for the integer formats; Float32 keeps
// every value (NaN and infinity included, so a test can see what a broken render produced).
inline std::vector<std::uint8_t> encodeWav(const WavData& wav, WavFormat format = WavFormat::Float32) {
    const int bytesPer = format == WavFormat::Float32 ? 4 : format == WavFormat::Pcm24 ? 3 : 2;
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(wav.samples.size() * static_cast<std::size_t>(bytesPer));
    const std::uint32_t channels = static_cast<std::uint32_t>(wav.channels);
    const std::uint32_t rate = static_cast<std::uint32_t>(wav.sampleRate);
    std::vector<std::uint8_t> out;
    out.reserve(44 + dataBytes + 1);
    detail::putTag(out, "RIFF");
    detail::putLe(out, 36 + dataBytes + (dataBytes & 1), 4);
    detail::putTag(out, "WAVE");
    detail::putTag(out, "fmt ");
    detail::putLe(out, 16, 4);
    detail::putLe(out, format == WavFormat::Float32 ? 3 : 1, 2);
    detail::putLe(out, channels, 2);
    detail::putLe(out, rate, 4);
    detail::putLe(out, rate * channels * static_cast<std::uint32_t>(bytesPer), 4);
    detail::putLe(out, channels * static_cast<std::uint32_t>(bytesPer), 2);
    detail::putLe(out, static_cast<std::uint32_t>(bytesPer * 8), 2);
    detail::putTag(out, "data");
    detail::putLe(out, dataBytes, 4);
    for (float v : wav.samples) {
        if (format == WavFormat::Float32) {
            std::uint32_t bits;
            std::memcpy(&bits, &v, 4);
            detail::putLe(out, bits, 4);
        } else if (format == WavFormat::Pcm24) {
            const long s = std::lround(std::clamp(v, -1.f, 1.f) * 8388607.f);
            detail::putLe(out, static_cast<std::uint32_t>(s) & 0xFFFFFF, 3);
        } else {
            const long s = std::lround(std::clamp(v, -1.f, 1.f) * 32767.f);
            detail::putLe(out, static_cast<std::uint32_t>(s) & 0xFFFF, 2);
        }
    }
    if (dataBytes & 1)
        out.push_back(0); // chunks are word aligned
    return out;
}

inline bool writeWav(const std::string& path, const WavData& wav, WavFormat format = WavFormat::Float32,
                     std::string* error = nullptr) {
    if (wav.channels <= 0 || wav.sampleRate <= 0)
        return detail::fail(error, "wav: channels and sample rate must be positive");
    const std::vector<std::uint8_t> bytes = encodeWav(wav, format);
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f)
        return detail::fail(error, "wav: cannot open the file for writing");
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    const bool closed = std::fclose(f) == 0;
    return (ok && closed) || detail::fail(error, "wav: write failed");
}

inline bool decodeWav(const std::uint8_t* d, std::size_t size, WavData& out, std::string* error = nullptr) {
    using detail::fail;
    using detail::getLe;
    out = WavData{};
    if (size < 12 || std::memcmp(d, "RIFF", 4) != 0 || std::memcmp(d + 8, "WAVE", 4) != 0)
        return fail(error, "wav: not a RIFF/WAVE file");
    int tag = 0, channels = 0, bits = 0;
    std::uint32_t rate = 0;
    bool haveFmt = false;
    std::size_t pos = 12;
    while (pos + 8 <= size) {
        const std::uint32_t len = getLe(d + pos + 4, 4);
        const std::size_t body = pos + 8;
        const std::size_t avail = size - body;
        if (std::memcmp(d + pos, "fmt ", 4) == 0) {
            if (len < 16 || avail < 16)
                return fail(error, "wav: fmt chunk too short");
            tag = static_cast<int>(getLe(d + body, 2));
            channels = static_cast<int>(getLe(d + body + 2, 2));
            rate = getLe(d + body + 4, 4);
            bits = static_cast<int>(getLe(d + body + 14, 2));
            if (tag == 0xFFFE && len >= 26 && avail >= 26)
                tag = static_cast<int>(getLe(d + body + 24, 2)); // the sub-format's leading format code
            haveFmt = true;
        } else if (std::memcmp(d + pos, "data", 4) == 0) {
            if (!haveFmt)
                return fail(error, "wav: data before fmt");
            if (channels <= 0 || rate == 0)
                return fail(error, "wav: bad channel count or sample rate");
            const bool isFloat = tag == 3;
            if (!((tag == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) ||
                  (isFloat && (bits == 32 || bits == 64))))
                return fail(error, "wav: unsupported sample format");
            const std::size_t bytesPer = static_cast<std::size_t>(bits / 8);
            // A streamed file may carry 0 or 0xFFFFFFFF as the size; use what is there.
            const std::size_t dataLen = std::min<std::size_t>(len, avail);
            const std::size_t count =
                dataLen / bytesPer / static_cast<std::size_t>(channels) * static_cast<std::size_t>(channels);
            out.channels = channels;
            out.sampleRate = static_cast<int>(rate);
            out.samples.resize(count);
            const std::uint8_t* p = d + body;
            for (std::size_t i = 0; i < count; ++i, p += bytesPer) {
                float v;
                if (isFloat && bits == 32) {
                    const std::uint32_t u = getLe(p, 4);
                    std::memcpy(&v, &u, 4);
                } else if (isFloat) {
                    std::uint64_t u =
                        static_cast<std::uint64_t>(getLe(p, 4)) | static_cast<std::uint64_t>(getLe(p + 4, 4)) << 32;
                    double x;
                    std::memcpy(&x, &u, 8);
                    v = static_cast<float>(x);
                } else if (bits == 8) {
                    v = (static_cast<int>(p[0]) - 128) / 128.f;
                } else if (bits == 16) {
                    v = static_cast<std::int16_t>(getLe(p, 2)) / 32768.f;
                } else if (bits == 24) {
                    std::uint32_t u = getLe(p, 3);
                    if (u & 0x800000)
                        u |= 0xFF000000u;
                    v = static_cast<float>(static_cast<std::int32_t>(u)) / 8388608.f;
                } else {
                    v = static_cast<float>(static_cast<double>(static_cast<std::int32_t>(getLe(p, 4))) / 2147483648.0);
                }
                out.samples[i] = v;
            }
            return true;
        }
        if (len > avail)
            break;
        pos = body + len + (len & 1);
    }
    return fail(error, haveFmt ? "wav: no data chunk" : "wav: no fmt chunk");
}

inline bool readWav(const std::string& path, WavData& out, std::string* error = nullptr) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return detail::fail(error, "wav: cannot open the file");
    std::vector<std::uint8_t> bytes;
    std::uint8_t buf[1 << 15];
    for (std::size_t n; (n = std::fread(buf, 1, sizeof(buf), f)) > 0;)
        bytes.insert(bytes.end(), buf, buf + n);
    std::fclose(f);
    return decodeWav(bytes.data(), bytes.size(), out, error);
}

} // namespace spux::inline v1::testing
