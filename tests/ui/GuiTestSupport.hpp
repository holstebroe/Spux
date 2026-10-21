// Shared by the UI tests (Modern and Classic): a host over the test parameter table, and image helpers for
// golden frames (portable pixmaps, structural similarity).
#pragma once

#include <doctest.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "params/TestParams.hpp"
#include "spux/ui/EditorHost.hpp"

namespace ui_test {

using namespace spux::ui;
using namespace testparams;
using spux::params::EditQueue;
using spux::params::ParamEdit;
using spux::params::ParamStore;

struct Host final : StoreEditorHost<kCount> {
    ParamStore<kCount> store{kTable};
    EditQueue queue;
    int flushes = 0;
    ModulationHost* mod = nullptr;
    PresetHost* presetHost = nullptr;
    Host() : StoreEditorHost(store, queue) {}
    void requestFlush() override { ++flushes; }
    ModulationHost* modulation() override { return mod; }
    PresetHost* presets() override { return presetHost; }
    double plainOf(std::uint32_t id) { return store.plain(kTable.indexOf(id)); }
    void set(std::uint32_t id, double plain) { store.setPlain(kTable.indexOf(id), plain); }
    std::vector<ParamEdit::Kind> drain() {
        std::vector<ParamEdit::Kind> k;
        ParamEdit e;
        while (queue.pop(e))
            k.push_back(e.kind);
        return k;
    }
};

// --- images --------------------------------------------------------------------------------------------

inline bool writePpm(const std::string& path, const std::vector<std::uint32_t>& px, int w, int h) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (std::uint32_t p : px) {
        const unsigned char c[3] = {static_cast<unsigned char>(p >> 16), static_cast<unsigned char>(p >> 8),
                                    static_cast<unsigned char>(p)};
        std::fwrite(c, 1, 3, f);
    }
    std::fclose(f);
    return true;
}

inline bool readPpm(const std::string& path, std::vector<std::uint32_t>& px, int& w, int& h) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    int maxv = 0;
    bool ok = std::fscanf(f, "P6 %d %d %d", &w, &h, &maxv) == 3 && std::fgetc(f) != EOF;
    if (ok) {
        px.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
        for (auto& p : px) {
            unsigned char c[3];
            if (std::fread(c, 1, 3, f) != 3) {
                ok = false;
                break;
            }
            p = 0xFF000000u | (static_cast<std::uint32_t>(c[0]) << 16) | (static_cast<std::uint32_t>(c[1]) << 8) | c[2];
        }
    }
    std::fclose(f);
    return ok;
}

// The mean of each 4 x 4 block: the size of the golden files.
inline std::vector<std::uint32_t> quarter(const std::vector<std::uint32_t>& px, int w, int h) {
    std::vector<std::uint32_t> out(static_cast<std::size_t>(w / 4) * static_cast<std::size_t>(h / 4));
    for (int y = 0; y < h / 4; ++y)
        for (int x = 0; x < w / 4; ++x) {
            unsigned r = 0, g = 0, b = 0;
            for (int dy = 0; dy < 4; ++dy)
                for (int dx = 0; dx < 4; ++dx) {
                    const std::uint32_t p = px[static_cast<std::size_t>(y * 4 + dy) * static_cast<std::size_t>(w) +
                                               static_cast<std::size_t>(x * 4 + dx)];
                    r += (p >> 16) & 255u;
                    g += (p >> 8) & 255u;
                    b += p & 255u;
                }
            out[static_cast<std::size_t>(y) * static_cast<std::size_t>(w / 4) + static_cast<std::size_t>(x)] =
                0xFF000000u | ((r / 16) << 16) | ((g / 16) << 8) | (b / 16);
        }
    return out;
}

// Mean structural similarity over 8 x 8 windows of the luma (standard constants).
inline double ssim(const std::vector<std::uint32_t>& a, const std::vector<std::uint32_t>& b, int w, int h) {
    auto luma = [&](const std::vector<std::uint32_t>& v, int x, int y) {
        const std::uint32_t p =
            v[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)];
        return 0.299 * ((p >> 16) & 255u) + 0.587 * ((p >> 8) & 255u) + 0.114 * (p & 255u);
    };
    const double c1 = 6.5025, c2 = 58.5225;
    double sum = 0;
    int n = 0;
    for (int by = 0; by + 8 <= h; by += 8)
        for (int bx = 0; bx + 8 <= w; bx += 8) {
            double ma = 0, mb = 0, saa = 0, sbb = 0, sab = 0;
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    const double va = luma(a, bx + x, by + y), vb = luma(b, bx + x, by + y);
                    ma += va;
                    mb += vb;
                    saa += va * va;
                    sbb += vb * vb;
                    sab += va * vb;
                }
            ma /= 64;
            mb /= 64;
            const double va = saa / 64 - ma * ma, vb = sbb / 64 - mb * mb, cov = sab / 64 - ma * mb;
            sum += ((2 * ma * mb + c1) * (2 * cov + c2)) / ((ma * ma + mb * mb + c1) * (va + vb + c2));
            ++n;
        }
    return n ? sum / n : 1.0;
}

inline int luma(std::uint32_t p) { return static_cast<int>(((p >> 16) & 255u) + ((p >> 8) & 255u) + (p & 255u)) / 3; }

// Compares a frame with its stored golden (a quarter-size ppm in tests/golden/). SPUX_UPDATE_GOLDEN=1 writes it.
// A missing golden is reported, not failed, so a new case can be added before its file exists.
inline void checkGolden(const std::string& name, const std::vector<std::uint32_t>& frame, int w, int h,
                        [[maybe_unused]] double minSimilarity) {
    const std::string path = std::string(SPUX_TEST_DATA_DIR) + "/golden/" + name + ".ppm";
    const auto small = quarter(frame, w, h);
    if (std::getenv("SPUX_UPDATE_GOLDEN")) {
        std::filesystem::create_directories(std::string(SPUX_TEST_DATA_DIR) + "/golden");
        CHECK(writePpm(path, small, w / 4, h / 4));
        return;
    }
    std::vector<std::uint32_t> ref;
    int rw = 0, rh = 0;
    if (!readPpm(path, ref, rw, rh)) {
        std::printf("golden frame %s missing (SPUX_UPDATE_GOLDEN=1 creates it)\n", name.c_str());
        return;
    }
    REQUIRE(rw == w / 4);
    REQUIRE(rh == h / 4);
    [[maybe_unused]] const double s = ssim(small, ref, rw, rh);
    INFO(name << " similarity " << s);
    CHECK(s >= minSimilarity);
}

// SPUX_SNAPSHOT_DIR=<dir> writes full-size frames there (to look at, or for documentation screenshots).
inline void snapshot(const std::string& name, const std::vector<std::uint32_t>& frame, int w, int h) {
    if (const char* dir = std::getenv("SPUX_SNAPSHOT_DIR")) {
        std::filesystem::create_directories(dir);
        writePpm(std::string(dir) + "/" + name + ".ppm", frame, w, h);
    }
}

} // namespace ui_test
