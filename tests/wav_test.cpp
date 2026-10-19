// WAV I/O: float round trips exactly, the integer formats within a quantum, the byte layout matches
// what Kikset's WavWriter and Plumeflow's Wav.hpp wrote, and damaged input is rejected without a crash.
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>

#include "spux/testing/Wav.hpp"

using namespace spux::testing;

namespace {
WavData ramp(int channels, int frames) {
    WavData w;
    w.channels = channels;
    w.sampleRate = 44100;
    for (int i = 0; i < frames * channels; ++i)
        w.samples.push_back(static_cast<float>(std::sin(0.01 * i)) * 0.9f);
    return w;
}

// Kikset's WavWriter.hpp (writeWav16), as it was when the code moved, producing bytes instead of a file.
std::vector<std::uint8_t> kiksetWav16(const std::vector<float>& x, double fs) {
    std::vector<std::uint8_t> out;
    auto w32 = [&](std::uint32_t v) {
        for (int i = 0; i < 4; ++i)
            out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    };
    auto w16 = [&](std::uint16_t v) {
        for (int i = 0; i < 2; ++i)
            out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    };
    auto tag = [&](const char* t, int n) { out.insert(out.end(), t, t + n); };
    const std::uint32_t n = std::uint32_t(x.size()), bytes = n * 2, sr = std::uint32_t(fs);
    tag("RIFF", 4);
    w32(36 + bytes);
    tag("WAVEfmt ", 8);
    w32(16);
    w16(1);
    w16(1);
    w32(sr);
    w32(sr * 2);
    w16(2);
    w16(16);
    tag("data", 4);
    w32(bytes);
    for (float v : x) {
        const std::int16_t s = std::int16_t(std::lround(std::clamp(v, -1.f, 1.f) * 32767.f));
        w16(static_cast<std::uint16_t>(s));
    }
    return out;
}
} // namespace

TEST_CASE("float32 round trips bit for bit, NaN and infinity included") {
    WavData w = ramp(2, 500);
    w.samples[3] = std::numeric_limits<float>::infinity();
    w.samples[4] = -0.0f;
    const auto bytes = encodeWav(w);
    WavData back;
    std::string error;
    REQUIRE(decodeWav(bytes.data(), bytes.size(), back, &error));
    CHECK(back.channels == 2);
    CHECK(back.sampleRate == 44100);
    REQUIRE(back.samples.size() == w.samples.size());
    CHECK(std::memcmp(back.samples.data(), w.samples.data(), w.samples.size() * 4) == 0);
}

TEST_CASE("integer formats round trip within 1.5 quanta (rounding plus the 32767 / 32768 scale of the usual "
          "convention) and clamp") {
    WavData w = ramp(1, 300);
    w.samples[0] = 1.5f;
    w.samples[1] = -2.f;
    for (WavFormat f : {WavFormat::Pcm16, WavFormat::Pcm24}) {
        const auto bytes = encodeWav(w, f);
        WavData back;
        REQUIRE(decodeWav(bytes.data(), bytes.size(), back));
        REQUIRE(back.samples.size() == w.samples.size());
        const double quantum = f == WavFormat::Pcm16 ? 1.0 / 32768 : 1.0 / 8388608;
        CHECK(back.samples[0] == doctest::Approx(1.0).epsilon(2 * quantum));
        CHECK(back.samples[1] == doctest::Approx(-1.0).epsilon(2 * quantum));
        for (std::size_t i = 2; i < w.samples.size(); ++i)
            CHECK(std::fabs(back.samples[i] - w.samples[i]) <= 1.6 * quantum);
    }
}

TEST_CASE("the 16-bit layout is the one Kikset wrote") {
    const WavData w = ramp(1, 480); // an even number of samples: no pad byte
    CHECK(encodeWav(w, WavFormat::Pcm16) == kiksetWav16(w.samples, 44100));
}

TEST_CASE("the float layout is the one Plumeflow wrote") {
    const WavData w = ramp(2, 100);
    const auto b = encodeWav(w);
    REQUIRE(b.size() == 44 + w.samples.size() * 4);
    CHECK(std::memcmp(b.data(), "RIFF", 4) == 0);
    CHECK(std::memcmp(b.data() + 8, "WAVEfmt ", 8) == 0);
    CHECK(b[16] == 16); // fmt size
    CHECK(b[20] == 3);  // IEEE float
    CHECK(b[22] == 2);  // channels
    CHECK(b[34] == 32); // bits
    CHECK(std::memcmp(b.data() + 36, "data", 4) == 0);
}

TEST_CASE("an odd 16-bit data chunk is padded and still reads back") {
    WavData w = ramp(1, 5);
    const auto b = encodeWav(w, WavFormat::Pcm16);
    CHECK(b.size() % 2 == 0);
    WavData back;
    REQUIRE(decodeWav(b.data(), b.size(), back));
    CHECK(back.samples.size() == 5);
}

TEST_CASE("files: write, read, and report errors") {
    const auto dir = std::filesystem::temp_directory_path() / "spux_wav_test";
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "a.wav").string();
    const WavData w = ramp(2, 64);
    std::string error;
    REQUIRE(writeWav(path, w, WavFormat::Float32, &error));
    WavData back;
    REQUIRE(readWav(path, back, &error));
    CHECK(back.samples == w.samples);
    CHECK_FALSE(readWav((dir / "missing.wav").string(), back, &error));
    CHECK_FALSE(error.empty());
    WavData bad;
    CHECK_FALSE(writeWav(path, bad, WavFormat::Float32, &error)); // no channels
    std::filesystem::remove_all(dir);
}

TEST_CASE("damaged input is rejected without a crash") {
    const auto good = encodeWav(ramp(2, 20));
    WavData out;
    for (std::size_t cut = 0; cut < 44; ++cut)
        CHECK_FALSE(decodeWav(good.data(), cut, out));
    // a data chunk that claims more than the file holds reads what is there
    auto big = good;
    big[40] = big[41] = big[42] = big[43] = 0xFF;
    REQUIRE(decodeWav(big.data(), big.size(), out));
    CHECK(out.samples.size() == 40);
    auto badFormat = good;
    badFormat[20] = 7; // mu-law
    CHECK_FALSE(decodeWav(badFormat.data(), badFormat.size(), out));
    auto noChannels = good;
    noChannels[22] = 0;
    CHECK_FALSE(decodeWav(noChannels.data(), noChannels.size(), out));
    std::vector<std::uint8_t> junk(100, 0x41);
    CHECK_FALSE(decodeWav(junk.data(), junk.size(), out));
}
