// spux_clap_smoke <plugin.clap> [options]: load a built CLAP the way a host does and run the checks every
// plugin shares (see spux/testing/ClapHost.hpp). Exit code 0 when all pass. Use spux_add_clap_smoke_test()
// from cmake/SpuxTesting.cmake to register it as a ctest entry.
#include "spux/testing/ClapHost.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace spux::testing;

namespace {
int usage() {
    std::fputs("usage: spux_clap_smoke <plugin.clap> [--id <clap id>] [--plugins N] [--params N]\n"
               "       [--rate Hz] [--block N] [--blocks N] [--require-audio] [--max-peak X]\n"
               "       [--input silence|impulse|noise] [--no-param-event] [--no-state-stable] [--no-gui]\n",
               stderr);
    return 2;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2)
        return usage();
    SmokeOptions o;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        const bool hasValue = i + 1 < argc;
        if (a == "--id" && hasValue)
            o.pluginId = argv[++i];
        else if (a == "--plugins" && hasValue)
            o.pluginCount = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        else if (a == "--params" && hasValue)
            o.paramCount = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        else if (a == "--rate" && hasValue)
            o.sampleRate = std::atof(argv[++i]);
        else if (a == "--block" && hasValue)
            o.blockSize = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        else if (a == "--blocks" && hasValue)
            o.blocks = std::atoi(argv[++i]);
        else if (a == "--max-peak" && hasValue)
            o.maxPeak = std::atof(argv[++i]);
        else if (a == "--require-audio")
            o.minPeak = 1e-4;
        else if (a == "--input" && hasValue) {
            const std::string v = argv[++i];
            if (v == "silence")
                o.input = SmokeInput::Silence;
            else if (v == "impulse")
                o.input = SmokeInput::Impulse;
            else if (v == "noise")
                o.input = SmokeInput::Noise;
            else
                return usage();
        } else if (a == "--no-param-event")
            o.paramEvent = false;
        else if (a == "--no-state-stable")
            o.stateRoundTrip = false;
        else if (a == "--no-gui")
            o.gui = false;
        else
            return usage();
    }
    const SmokeReport r = runClapSmoke(argv[1], o);
    std::fputs(r.text().c_str(), r.ok() ? stdout : stderr);
    return r.ok() ? 0 : 1;
}
