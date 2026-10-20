// A minimal CLAP host for tests: loads a built .clap the way a DAW would, drives the real entry point,
// and runs the checks every plugin shares (entry and factory, parameters, ports, state round trip,
// processing with notes and parameter events, the X11 GUI when a display exists). Plugin-specific checks
// go in SmokeOptions::extra, which gets the live plugin.
//
// Checks are collected, not asserted, so they also run in Release builds and one run reports everything.
// The same code is the command line tool `spux_clap_smoke`.
#pragma once

#include <clap/clap.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace spux::inline v1::testing {

struct SmokeReport {
    int checks = 0;
    std::vector<std::string> failures;
    std::vector<std::string> notes; // parts that were skipped, host log lines at warning level or above

    bool ok() const { return failures.empty(); }
    void check(bool condition, const std::string& what) {
        ++checks;
        if (!condition)
            failures.push_back(what);
    }
    std::string text() const;
};

// What the host did, as seen by a plugin's extra checks.
struct HostActivity {
    int timerRegistrations = 0;
    int timerUnregistrations = 0;
    int paramFlushRequests = 0;
    int paramRescans = 0;
    int restartRequests = 0;
    int stateDirtyMarks = 0;
    int resizeRequests = 0;
    std::vector<clap_id> timerIds; // currently registered
};

struct SmokeNote {
    std::uint32_t frame = 0; // absolute sample at which the event happens
    bool on = true;
    int key = 60;
    double velocity = 0.8;
};

enum class SmokeInput { Auto, Silence, Impulse, Noise };

// Events and host state are owned by the smoke run; the pointers are valid inside `extra` only.
struct SmokeSession {
    const clap_plugin_t* plugin = nullptr;
    const clap_plugin_descriptor_t* descriptor = nullptr;
    const HostActivity* host = nullptr;
    double sampleRate = 48000.0;
    template <typename T> const T* extension(const char* id) const {
        return static_cast<const T*>(plugin->get_extension(plugin, id));
    }
};

struct SmokeOptions {
    std::string pluginId;                     // empty: the first plugin of the factory
    std::optional<std::uint32_t> pluginCount; // expected number of plugins in the factory
    std::optional<std::uint32_t> paramCount;  // expected number of parameters
    double sampleRate = 48000.0;
    std::uint32_t minFrames = 32, maxFrames = 4096;
    std::uint32_t blockSize = 512;
    int blocks = 100;
    double tempo = 120.0;                // the transport the host reports (playing, with a beat timeline)
    SmokeInput input = SmokeInput::Auto; // Auto: noise for an effect (audio in, no note in), else silence
    std::vector<SmokeNote> notes;        // empty: a C major chord for a plugin with a note input
    double maxPeak = 1.0;                // output must stay below this ...
    double minPeak = 0.0;                // ... and reach this (> 0: the plugin must make sound)
    bool paramEvent = true;              // send a parameter value event and read it back
    bool stateRoundTrip = true;          // save, load, save again: the two blobs must be equal
    bool textRoundTrip = true;           // value_to_text / text_to_value agree where the plugin offers them
    bool gui = true;                     // X11 embedding, only with a display and X11 compiled in
    std::function<void(const SmokeSession&, SmokeReport&)> extra; // plugin checks, called while processing is active
};

// Loads `clapPath` and runs the smoke sequence. Never throws; every problem is a failure in the report.
SmokeReport runClapSmoke(const std::string& clapPath, const SmokeOptions& options = {});

} // namespace spux::inline v1::testing
