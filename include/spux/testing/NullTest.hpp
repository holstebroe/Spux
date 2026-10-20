// Reference-render null test (guidelines section 3). A plugin renders a curated case (patch plus
// note sequence, a few seconds), and this compares the render with the stored float WAV in the plugin
// repository: subtract, report the peak and RMS of the difference, and fail above the threshold. The
// threshold is a parameter: -90 dBFS peak for a normal plugin, -110 dBFS for a strict one, -infinity for a
// bit-exact check.
//
//   NullOptions opt;
//   opt.referenceDir = SOURCE_DIR "/test/reference";
//   opt.peakThresholdDb = -90.0;
//   for (const Case& c : cases) results.push_back(runNullTest(c.name, render(c), opt));
//   for (const NullResult& r : results) CHECK_MESSAGE(r.passed(), nullMessage(r));
//   MESSAGE(formatNullReport(results, opt));
//
// Accepting a change means regenerating the references in the same commit: run with
// SPUX_UPDATE_REFERENCES=1 (or NullOptions::update). The report then lists what each case changed by.
// A case without a stored reference fails; it never passes silently.
//
// Sound tolerances: the peak difference detects any change, down to one ulp, but on a sensitive
// patch (a resonant ladder) a one-ulp change of a libm function already moves the output by -70 to
// -90 dBFS, inaudibly. To accept such changes and still catch real mistakes, set the tolerances on what
// is heard: maxSpectralDb (the worst short-time spectrum difference, dB) and maxEnvelopeDb (the worst
// 10 ms level difference, dB); peakThresholdDb = +infinity makes the peak a report only. A case passes
// when it is bit-identical or within every tolerance that is set.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "spux/testing/Hash.hpp"
#include "spux/testing/Metrics.hpp"
#include "spux/testing/Wav.hpp"

namespace spux::inline v1::testing {

struct NullOptions {
    std::string referenceDir;             // <dir>/<case>.wav
    double peakThresholdDb = -90.0;       // difference peak, dBFS; -infinity means bit-exact
    std::optional<double> rmsThresholdDb; // difference RMS relative to the reference RMS, dB; off when unset
    bool update = updateRequested("SPUX_UPDATE_REFERENCES"); // write the references instead of failing
    std::string diffDir; // when set, a failing case writes <case>.render.wav and <case>.diff.wav
    // Sound tolerances (see the top of this file); measured and checked when set.
    std::optional<double> maxSpectralDb; // worst frame's power-weighted mean |dB(render) - dB(reference)|
    std::optional<double> maxEnvelopeDb; // worst |dB| difference of the 10 ms RMS level
};

enum class NullStatus {
    Pass,       // within the threshold
    Fail,       // above the threshold, or a different format or length, or non-finite samples
    Missing,    // no stored reference (and not updating)
    Updated,    // the reference was (re)written
    Unreadable, // the stored file cannot be read
};

struct NullResult {
    std::string name;
    NullStatus status = NullStatus::Fail;
    double diffPeakDb = -std::numeric_limits<double>::infinity();   // dBFS
    double diffRmsDb = -std::numeric_limits<double>::infinity();    // dBFS
    double diffRmsRelDb = -std::numeric_limits<double>::infinity(); // relative to the reference RMS
    double refPeakDb = -std::numeric_limits<double>::infinity();
    double refRmsDb = -std::numeric_limits<double>::infinity();
    std::size_t peakFrame = 0; // where the largest difference is
    int peakChannel = 0;
    double spectralDb = -1.0; // sound measures, when a tolerance asked for them (-1: not measured)
    double envelopeDb = -1.0;
    bool compared = false; // the difference numbers are valid
    std::string note;      // why a case failed other than by the threshold

    bool passed() const { return status == NullStatus::Pass || status == NullStatus::Updated; }
};

namespace detail {

inline double toDb(double linear) {
    return linear > 0.0 ? 20.0 * std::log10(linear) : -std::numeric_limits<double>::infinity();
}

inline std::string safeName(const std::string& name) {
    std::string out;
    for (char c : name) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
                        c == '-' || c == '_' || c == '+' || c == '#';
        out += ok ? c : '_';
    }
    return out.empty() ? "case" : out;
}

inline std::string referencePath(const NullOptions& o, const std::string& name) {
    return o.referenceDir + "/" + safeName(name) + ".wav";
}

// Differences over the frames both files have; the reference RMS is over the same frames.
inline void measure(const WavData& ref, const WavData& got, NullResult& r, std::vector<float>* diff) {
    const std::size_t frames = std::min(ref.frames(), got.frames());
    const std::size_t ch = static_cast<std::size_t>(ref.channels);
    double dPeak = 0.0, rPeak = 0.0, dSum = 0.0, rSum = 0.0;
    if (diff)
        diff->assign(frames * ch, 0.f);
    for (std::size_t i = 0; i < frames * ch; ++i) {
        const double a = ref.samples[i], b = got.samples[i], d = b - a;
        if (diff)
            (*diff)[i] = static_cast<float>(d);
        if (std::fabs(d) > dPeak) {
            dPeak = std::fabs(d);
            r.peakFrame = i / ch;
            r.peakChannel = static_cast<int>(i % ch);
        }
        rPeak = std::max(rPeak, std::fabs(a));
        dSum += d * d;
        rSum += a * a;
    }
    const double n = static_cast<double>(std::max<std::size_t>(frames * ch, 1));
    const double dRms = std::sqrt(dSum / n), rRms = std::sqrt(rSum / n);
    r.diffPeakDb = toDb(dPeak);
    r.diffRmsDb = toDb(dRms);
    r.refPeakDb = toDb(rPeak);
    r.refRmsDb = toDb(rRms);
    r.diffRmsRelDb = dRms == 0.0   ? -std::numeric_limits<double>::infinity()
                     : rRms == 0.0 ? std::numeric_limits<double>::infinity()
                                   : toDb(dRms / rRms);
    r.compared = true;
}

// Sound measures between a reference and a render of the same format, the worst over the channels.
// Spectrum: 2048-point Hann frames, hop 1024, the frames within 40 dB of the reference's loudest; a
// frame's value is the mean |dB difference| of its bins weighted by the reference's power in each bin
// (quiet bins, where rounding noise moves the dB most, count little). Envelope: the 10 ms RMS levels
// within 40 dB of the reference's loudest.
inline void soundMeasures(const WavData& ref, const WavData& got, NullResult& r) {
    const std::size_t ch = static_cast<std::size_t>(ref.channels), frames = ref.frames();
    const auto toDbPow = [](double p) { return 10.0 * std::log10(p + 1e-30); };
    double spec = 0.0, env = 0.0;
    for (std::size_t c = 0; c < ch; ++c) {
        std::vector<double> a(frames), b(frames);
        for (std::size_t i = 0; i < frames; ++i) {
            a[i] = ref.samples[i * ch + c];
            b[i] = got.samples[i * ch + c];
        }
        const std::size_t block = std::max<std::size_t>(1, static_cast<std::size_t>(ref.sampleRate / 100));
        std::vector<double> la, lb;
        for (std::size_t i = 0; i + block <= frames; i += block) {
            double sa = 0.0, sb = 0.0;
            for (std::size_t k = i; k < i + block; ++k) {
                sa += a[k] * a[k];
                sb += b[k] * b[k];
            }
            la.push_back(toDbPow(sa / block));
            lb.push_back(toDbPow(sb / block));
        }
        const double laMax = la.empty() ? -300.0 : *std::max_element(la.begin(), la.end());
        for (std::size_t i = 0; i < la.size(); ++i)
            if (la[i] > laMax - 40.0 && la[i] > -120.0)
                env = std::max(env, std::fabs(la[i] - lb[i]));

        constexpr std::size_t kN = 2048, kHop = 1024;
        std::vector<std::vector<double>> pa, pb;
        std::vector<double> energy;
        for (std::size_t i = 0; i + kN <= frames; i += kHop) {
            pa.push_back(powerSpectrum(a, i, i + kN));
            pb.push_back(powerSpectrum(b, i, i + kN));
            double e = 0.0;
            for (double v : pa.back())
                e += v;
            energy.push_back(toDbPow(e));
        }
        const double eMax = energy.empty() ? -300.0 : *std::max_element(energy.begin(), energy.end());
        for (std::size_t f = 0; f < pa.size(); ++f) {
            if (energy[f] < eMax - 40.0 || energy[f] < -200.0)
                continue;
            double num = 0.0, den = 0.0;
            for (std::size_t k = 0; k < pa[f].size(); ++k) {
                num += pa[f][k] * std::fabs(toDbPow(pa[f][k]) - toDbPow(pb[f][k]));
                den += pa[f][k];
            }
            if (den > 0.0)
                spec = std::max(spec, num / den);
        }
    }
    r.spectralDb = spec;
    r.envelopeDb = env;
}

inline bool sameFormat(const WavData& a, const WavData& b, std::string& why) {
    if (a.channels != b.channels)
        why = "channels " + std::to_string(a.channels) + " vs reference " + std::to_string(b.channels);
    else if (a.sampleRate != b.sampleRate)
        why = "sample rate " + std::to_string(a.sampleRate) + " vs reference " + std::to_string(b.sampleRate);
    else if (a.frames() != b.frames())
        why = "length " + std::to_string(a.frames()) + " frames vs reference " + std::to_string(b.frames());
    return why.empty();
}

} // namespace detail

// Compares one render with its stored reference, or records it when options.update is set.
inline NullResult runNullTest(const std::string& name, const WavData& rendered, const NullOptions& options) {
    NullResult r;
    r.name = name;
    const std::string path = detail::referencePath(options, name);

    for (float v : rendered.samples)
        if (!std::isfinite(v)) {
            r.status = NullStatus::Fail;
            r.note = "the render contains non-finite samples";
            return r;
        }
    if (rendered.channels <= 0 || rendered.sampleRate <= 0) {
        r.note = "the render has no channels or no sample rate";
        return r;
    }

    WavData ref;
    std::string error;
    std::error_code existsError;
    const bool exists = std::filesystem::exists(path, existsError);
    const bool readable = exists && readWav(path, ref, &error);

    if (readable) {
        std::string why;
        if (detail::sameFormat(rendered, ref, why)) {
            std::vector<float> diff;
            detail::measure(ref, rendered, r, &diff);
            const bool peakOk = r.diffPeakDb <= options.peakThresholdDb;
            const bool rmsOk = !options.rmsThresholdDb || r.diffRmsRelDb <= *options.rmsThresholdDb;
            bool soundOk = true;
            if (options.maxSpectralDb || options.maxEnvelopeDb) {
                if (std::isinf(r.diffPeakDb)) {
                    r.spectralDb = r.envelopeDb = 0.0; // bit-identical
                } else {
                    detail::soundMeasures(ref, rendered, r);
                    soundOk = (!options.maxSpectralDb || r.spectralDb <= *options.maxSpectralDb) &&
                              (!options.maxEnvelopeDb || r.envelopeDb <= *options.maxEnvelopeDb);
                }
            }
            r.status = peakOk && rmsOk && soundOk ? NullStatus::Pass : NullStatus::Fail;
            if (r.status == NullStatus::Fail && !options.diffDir.empty()) {
                std::error_code ec;
                std::filesystem::create_directories(options.diffDir, ec);
                WavData d = rendered;
                d.samples = diff;
                writeWav(options.diffDir + "/" + detail::safeName(name) + ".diff.wav", d);
                writeWav(options.diffDir + "/" + detail::safeName(name) + ".render.wav", rendered);
            }
        } else {
            r.status = NullStatus::Fail;
            r.note = why;
            if (rendered.channels == ref.channels)
                detail::measure(ref, rendered, r, nullptr); // over the frames both have
        }
    } else if (exists) {
        r.status = NullStatus::Unreadable;
        r.note = error;
    } else {
        r.status = NullStatus::Missing;
    }

    // Updating regenerates every reference that is not bit-identical, also those within the threshold, so
    // the stored files do not drift away from the code they accept.
    const bool identical = r.compared && r.status == NullStatus::Pass && std::isinf(r.diffPeakDb);
    if (options.update && !identical) {
        std::error_code ec;
        std::filesystem::create_directories(options.referenceDir, ec);
        if (writeWav(path, rendered, WavFormat::Float32, &error)) {
            r.status = NullStatus::Updated;
            r.note.clear();
        } else {
            r.status = NullStatus::Fail;
            r.note = error;
        }
    }
    return r;
}

inline const char* statusText(NullStatus s) {
    switch (s) {
    case NullStatus::Pass:
        return "pass";
    case NullStatus::Fail:
        return "FAIL";
    case NullStatus::Missing:
        return "MISSING";
    case NullStatus::Updated:
        return "updated";
    case NullStatus::Unreadable:
        return "UNREADABLE";
    }
    return "?";
}

namespace detail {
inline std::string db(double v) {
    char buf[32];
    if (std::isinf(v))
        std::snprintf(buf, sizeof(buf), "%s", v < 0 ? "-inf" : "+inf");
    else
        std::snprintf(buf, sizeof(buf), "%.1f", v);
    return buf;
}
inline std::string db2(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", v);
    return buf;
}
} // namespace detail

// One line, for a failed CHECK_MESSAGE.
inline std::string nullMessage(const NullResult& r) {
    std::string s = "null test '" + r.name + "': " + statusText(r.status);
    if (r.compared)
        s += ", difference peak " + detail::db(r.diffPeakDb) + " dBFS, rms " + detail::db(r.diffRmsDb) + " dBFS (" +
             detail::db(r.diffRmsRelDb) + " dB re reference) at frame " + std::to_string(r.peakFrame) + " channel " +
             std::to_string(r.peakChannel);
    if (r.spectralDb >= 0.0)
        s += ", spectrum " + detail::db2(r.spectralDb) + " dB, envelope " + detail::db2(r.envelopeDb) + " dB";
    if (!r.note.empty())
        s += " [" + r.note + "]";
    if (r.status == NullStatus::Missing)
        s += " [no reference; SPUX_UPDATE_REFERENCES=1 records it]";
    return s;
}

// A table, one row per case: the per-patch report of a strict gate.
inline std::string formatNullReport(const std::vector<NullResult>& results, const NullOptions& options) {
    std::string s = "null test, threshold " + detail::db(options.peakThresholdDb) + " dBFS peak";
    if (options.rmsThresholdDb)
        s += ", " + detail::db(*options.rmsThresholdDb) + " dB rms re reference";
    const bool sound = options.maxSpectralDb || options.maxEnvelopeDb;
    if (options.maxSpectralDb)
        s += ", spectrum within " + detail::db2(*options.maxSpectralDb) + " dB";
    if (options.maxEnvelopeDb)
        s += ", envelope within " + detail::db2(*options.maxEnvelopeDb) + " dB";
    s += "\n";
    char line[320];
    std::snprintf(line, sizeof(line), "%-32s %-10s %10s %10s %10s %10s%s\n", "case", "status", "diff peak", "diff rms",
                  "re ref", "ref peak", sound ? "   spectrum   envelope" : "");
    s += line;
    for (const NullResult& r : results) {
        char extra[64] = "";
        if (sound)
            std::snprintf(extra, sizeof(extra), " %10s %10s",
                          r.spectralDb >= 0.0 ? detail::db2(r.spectralDb).c_str() : "-",
                          r.envelopeDb >= 0.0 ? detail::db2(r.envelopeDb).c_str() : "-");
        std::snprintf(
            line, sizeof(line), "%-32s %-10s %10s %10s %10s %10s%s%s%s\n", r.name.c_str(), statusText(r.status),
            r.compared ? detail::db(r.diffPeakDb).c_str() : "-", r.compared ? detail::db(r.diffRmsDb).c_str() : "-",
            r.compared ? detail::db(r.diffRmsRelDb).c_str() : "-", r.compared ? detail::db(r.refPeakDb).c_str() : "-",
            extra, r.note.empty() ? "" : "  ", r.note.c_str());
        s += line;
    }
    return s;
}

} // namespace spux::inline v1::testing
