// The reference-render null test: record, compare, report, thresholds, and every way a case can fail.
#include <doctest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

#include "spux/testing/NullTest.hpp"

using namespace spux::testing;

namespace {
struct TempDir {
    std::filesystem::path path;
    explicit TempDir(const char* name) : path(std::filesystem::temp_directory_path() / name) {
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }
    ~TempDir() { std::filesystem::remove_all(path); }
};

WavData render(int frames = 4800, double gain = 0.5) {
    WavData w;
    w.channels = 2;
    w.sampleRate = 48000;
    for (int i = 0; i < frames; ++i) {
        const float v = static_cast<float>(gain * std::sin(0.05 * i));
        w.samples.push_back(v);
        w.samples.push_back(-v);
    }
    return w;
}

NullOptions options(const TempDir& d) {
    NullOptions o;
    o.referenceDir = (d.path / "ref").string();
    o.update = false;
    return o;
}
} // namespace

TEST_CASE("a case without a reference fails, and recording it makes it pass") {
    TempDir d("spux_null_test_a");
    NullOptions o = options(d);
    NullResult r = runNullTest("pad chord", render(), o);
    CHECK(r.status == NullStatus::Missing);
    CHECK_FALSE(r.passed());
    CHECK(nullMessage(r).find("SPUX_UPDATE_REFERENCES") != std::string::npos);

    o.update = true;
    r = runNullTest("pad chord", render(), o);
    CHECK(r.status == NullStatus::Updated);
    CHECK(r.passed());
    CHECK(std::filesystem::exists(d.path / "ref" / "pad_chord.wav")); // the name is made file-safe

    o.update = false;
    r = runNullTest("pad chord", render(), o);
    CHECK(r.status == NullStatus::Pass);
    CHECK(std::isinf(r.diffPeakDb));
    CHECK(r.diffPeakDb < 0);
    CHECK(r.refPeakDb == doctest::Approx(-6.02).epsilon(0.01));

    // updating an identical render reports a pass and leaves the file alone
    o.update = true;
    CHECK(runNullTest("pad chord", render(), o).status == NullStatus::Pass);
}

TEST_CASE("the threshold decides: -90 dBFS peak, -110 dBFS, and bit-exact") {
    TempDir d("spux_null_test_b");
    NullOptions o = options(d);
    o.update = true;
    runNullTest("x", render(), o);
    o.update = false;

    WavData off = render();
    off.samples[1000] += 1e-5f; // -100 dBFS
    NullResult r = runNullTest("x", off, o);
    CHECK(r.diffPeakDb == doctest::Approx(-100.0).epsilon(0.01));
    CHECK(r.peakFrame == 500);
    CHECK(r.peakChannel == 0);

    o.peakThresholdDb = -90.0;
    CHECK(runNullTest("x", off, o).status == NullStatus::Pass);
    o.peakThresholdDb = -110.0; // Acidus
    CHECK(runNullTest("x", off, o).status == NullStatus::Fail);
    o.peakThresholdDb = -std::numeric_limits<double>::infinity();
    CHECK(runNullTest("x", off, o).status == NullStatus::Fail);
    CHECK(runNullTest("x", render(), o).status == NullStatus::Pass);
}

TEST_CASE("the optional RMS threshold is relative to the reference") {
    TempDir d("spux_null_test_c");
    NullOptions o = options(d);
    o.update = true;
    runNullTest("x", render(), o);
    o.update = false;
    WavData off = render(4800, 0.5005); // 0.1 % louder: -60 dB relative
    NullResult r = runNullTest("x", off, o);
    CHECK(r.diffRmsRelDb == doctest::Approx(-60.0).epsilon(0.02));
    o.peakThresholdDb = 0.0; // so that only the RMS decides
    o.rmsThresholdDb = -50.0;
    CHECK(runNullTest("x", off, o).status == NullStatus::Pass);
    o.rmsThresholdDb = -70.0;
    CHECK(runNullTest("x", off, o).status == NullStatus::Fail); // the peak is fine, the RMS is not
}

TEST_CASE("a different format, length or non-finite sample fails with a reason") {
    TempDir d("spux_null_test_d");
    NullOptions o = options(d);
    o.update = true;
    runNullTest("x", render(), o);
    o.update = false;

    NullResult r = runNullTest("x", render(4000), o);
    CHECK(r.status == NullStatus::Fail);
    CHECK(r.note.find("length") != std::string::npos);
    CHECK(r.compared); // over the frames both have

    WavData mono = render();
    mono.channels = 1;
    r = runNullTest("x", mono, o);
    CHECK(r.status == NullStatus::Fail);
    CHECK(r.note.find("channels") != std::string::npos);

    WavData rate = render();
    rate.sampleRate = 44100;
    CHECK(runNullTest("x", rate, o).note.find("sample rate") != std::string::npos);

    WavData nan = render();
    nan.samples[10] = std::nanf("");
    r = runNullTest("x", nan, o);
    CHECK(r.status == NullStatus::Fail);
    CHECK(r.note.find("non-finite") != std::string::npos);
}

TEST_CASE("updating regenerates a changed render even within the threshold") {
    TempDir d("spux_null_test_e");
    NullOptions o = options(d);
    o.update = true;
    runNullTest("x", render(), o);
    WavData off = render();
    off.samples[5] += 1e-6f;
    NullResult r = runNullTest("x", off, o);
    CHECK(r.status == NullStatus::Updated);
    CHECK(r.compared); // the report shows what the update changed by
    CHECK(r.diffPeakDb < -100.0);
    o.update = false;
    o.peakThresholdDb = -std::numeric_limits<double>::infinity();
    CHECK(runNullTest("x", off, o).status == NullStatus::Pass); // the stored file is the new one
}

TEST_CASE("a failing case can leave the render and the difference to listen to") {
    TempDir d("spux_null_test_f");
    NullOptions o = options(d);
    o.diffDir = (d.path / "diff").string();
    o.update = true;
    runNullTest("x", render(), o);
    o.update = false;
    WavData off = render();
    off.samples[8] += 0.01f;
    CHECK(runNullTest("x", off, o).status == NullStatus::Fail);
    WavData diff;
    REQUIRE(readWav((d.path / "diff" / "x.diff.wav").string(), diff));
    CHECK(diff.samples[8] == doctest::Approx(0.01).epsilon(0.001));
    CHECK(std::filesystem::exists(d.path / "diff" / "x.render.wav"));
}

TEST_CASE("an unreadable reference is reported") {
    TempDir d("spux_null_test_g");
    NullOptions o = options(d);
    std::filesystem::create_directories(d.path / "ref");
    std::ofstream(d.path / "ref" / "x.wav") << "not a wav file at all, but long enough to look at";
    const NullResult r = runNullTest("x", render(), o);
    CHECK(r.status == NullStatus::Unreadable);
    CHECK_FALSE(r.passed());
}

TEST_CASE("the report has one row per case") {
    TempDir d("spux_null_test_h");
    NullOptions o = options(d);
    o.update = true;
    runNullTest("a", render(), o);
    o.update = false;
    std::vector<NullResult> results = {runNullTest("a", render(), o), runNullTest("b", render(), o)};
    const std::string report = formatNullReport(results, o);
    CHECK(report.find("-90.0 dBFS") != std::string::npos);
    CHECK(report.find("pass") != std::string::npos);
    CHECK(report.find("MISSING") != std::string::npos);
}

TEST_CASE("sound tolerances accept an inaudible change the peak threshold rejects, and catch a small real one") {
    TempDir d("spux_null_test_sound");
    NullOptions o = options(d);
    o.update = true;
    runNullTest("x", render(48000), o);
    o.update = false;
    o.peakThresholdDb = std::numeric_limits<double>::infinity(); // the peak is a report only
    o.maxSpectralDb = 0.05;
    o.maxEnvelopeDb = 0.05;

    NullResult r = runNullTest("x", render(48000), o);
    CHECK(r.status == NullStatus::Pass);
    CHECK(r.spectralDb == 0.0); // bit-identical: nothing to measure

    WavData tiny = render(48000); // one ulp-sized step on every 100th sample: about -100 dBFS peak
    for (std::size_t i = 0; i < tiny.samples.size(); i += 100)
        tiny.samples[i] = std::nextafter(tiny.samples[i], 1.0f) + 1e-5f;
    r = runNullTest("x", tiny, o);
    CHECK(r.diffPeakDb > -110.0); // the strict peak threshold would reject it
    CHECK(r.spectralDb < 0.01);
    CHECK(r.envelopeDb < 0.01);
    CHECK(r.status == NullStatus::Pass);

    WavData louder = render(48000, 0.5 * std::pow(10.0, 0.2 / 20.0)); // +0.2 dB
    r = runNullTest("x", louder, o);
    CHECK(r.envelopeDb == doctest::Approx(0.2).epsilon(0.02));
    CHECK(r.spectralDb == doctest::Approx(0.2).epsilon(0.02));
    CHECK(r.status == NullStatus::Fail);
    CHECK(nullMessage(r).find("spectrum") != std::string::npos);
    CHECK(formatNullReport({r}, o).find("envelope") != std::string::npos);
}
