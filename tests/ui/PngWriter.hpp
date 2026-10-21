// A minimal PNG writer (8-bit RGB, stored deflate blocks, no compression library needed) for documentation images.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace ui_test {

inline std::uint32_t crc32Png(const std::uint8_t* p, std::size_t n, std::uint32_t crc = 0xFFFFFFFFu) {
    for (std::size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (int k = 0; k < 8; ++k)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}

inline void putBe32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    for (int s = 24; s >= 0; s -= 8)
        v.push_back(static_cast<std::uint8_t>(x >> s));
}

inline void pngChunk(std::vector<std::uint8_t>& out, const char* type, const std::vector<std::uint8_t>& data) {
    putBe32(out, static_cast<std::uint32_t>(data.size()));
    std::vector<std::uint8_t> body(type, type + 4);
    body.insert(body.end(), data.begin(), data.end());
    out.insert(out.end(), body.begin(), body.end());
    putBe32(out, crc32Png(body.data(), body.size()) ^ 0xFFFFFFFFu);
}

// `px` is w x h of 0xAARRGGBB (alpha ignored); each pixel is drawn `zoom` times in both directions.
inline bool writePng(const std::string& path, const std::vector<std::uint32_t>& px, int w, int h, int zoom = 1) {
    const int W = w * zoom, H = h * zoom;
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(H) * (static_cast<std::size_t>(W) * 3 + 1));
    for (int y = 0; y < H; ++y) {
        raw.push_back(0); // filter: none
        for (int x = 0; x < W; ++x) {
            const std::uint32_t p = px[static_cast<std::size_t>(y / zoom) * static_cast<std::size_t>(w) +
                                       static_cast<std::size_t>(x / zoom)];
            raw.push_back(static_cast<std::uint8_t>(p >> 16));
            raw.push_back(static_cast<std::uint8_t>(p >> 8));
            raw.push_back(static_cast<std::uint8_t>(p));
        }
    }
    std::vector<std::uint8_t> z = {0x78, 0x01};
    for (std::size_t pos = 0; pos < raw.size() || pos == 0;) {
        const std::size_t n = std::min<std::size_t>(65535, raw.size() - pos);
        const bool last = pos + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<std::uint8_t>(n));
        z.push_back(static_cast<std::uint8_t>(n >> 8));
        z.push_back(static_cast<std::uint8_t>(~n));
        z.push_back(static_cast<std::uint8_t>((~n) >> 8));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                 raw.begin() + static_cast<std::ptrdiff_t>(pos + n));
        pos += n;
        if (last)
            break;
    }
    std::uint32_t a = 1, b = 0;
    for (std::uint8_t c : raw) {
        a = (a + c) % 65521u;
        b = (b + a) % 65521u;
    }
    putBe32(z, (b << 16) | a);

    std::vector<std::uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<std::uint8_t> ihdr;
    putBe32(ihdr, static_cast<std::uint32_t>(W));
    putBe32(ihdr, static_cast<std::uint32_t>(H));
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    pngChunk(out, "IHDR", ihdr);
    pngChunk(out, "IDAT", z);
    pngChunk(out, "IEND", {});
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    const bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

} // namespace ui_test
