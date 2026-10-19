// The smoke host against a small test plugin (tests/clap_test_plugin.cpp): the good build passes every
// check, and each broken build is caught by the check that is meant to catch it.
#include <doctest.h>

#include <algorithm>
#include <string>

#include "spux/testing/ClapHost.hpp"

using namespace spux::testing;

namespace {
bool failedWith(const SmokeReport& r, const std::string& part) {
    return std::any_of(r.failures.begin(), r.failures.end(),
                       [&](const std::string& f) { return f.find(part) != std::string::npos; });
}
} // namespace

TEST_CASE("a sound plugin passes the smoke host") {
    SmokeOptions o;
    o.pluginId = "org.spux.test-plugin";
    o.pluginCount = 1;
    o.paramCount = 2;
    o.minPeak = 0.01; // the chord must be heard
    const SmokeReport r = runClapSmoke(SPUX_TEST_PLUGIN_GOOD, o);
    INFO(r.text());
    CHECK(r.ok());
    CHECK(r.checks > 20);
}

TEST_CASE("the plugin's extra checks run while it is active") {
    SmokeOptions o;
    bool called = false;
    o.extra = [&](const SmokeSession& s, SmokeReport& r) {
        called = true;
        r.check(std::string(s.descriptor->id) == "org.spux.test-plugin", "descriptor id");
        r.check(s.extension<clap_plugin_params_t>(CLAP_EXT_PARAMS) != nullptr, "params extension");
    };
    const SmokeReport r = runClapSmoke(SPUX_TEST_PLUGIN_GOOD, o);
    CHECK(called);
    CHECK(r.ok());
}

TEST_CASE("wrong expectations are reported") {
    SmokeOptions o;
    o.paramCount = 3;
    o.pluginId = "org.spux.nothing";
    SmokeReport r = runClapSmoke(SPUX_TEST_PLUGIN_GOOD, o);
    CHECK(failedWith(r, "is not in the factory"));
    o.pluginId.clear();
    r = runClapSmoke(SPUX_TEST_PLUGIN_GOOD, o);
    CHECK(failedWith(r, "parameter count 2, expected 3"));
    o = SmokeOptions{};
    o.notes = {{0, true, 60, 0.8}}; // the note is held, so it sounds; an impossible minimum is caught
    o.minPeak = 5.0;
    CHECK(failedWith(runClapSmoke(SPUX_TEST_PLUGIN_GOOD, o), "required"));
}

TEST_CASE("a missing library is a failure, not a crash") {
    const SmokeReport r = runClapSmoke("/no/such/plugin.clap");
    CHECK_FALSE(r.ok());
    CHECK(failedWith(r, "cannot load"));
}

TEST_CASE("broken plugins are caught") {
    CHECK(failedWith(runClapSmoke(SPUX_TEST_PLUGIN_NAN), "non-finite"));
    CHECK(failedWith(runClapSmoke(SPUX_TEST_PLUGIN_LOUD), "is not below"));
    CHECK(failedWith(runClapSmoke(SPUX_TEST_PLUGIN_STATE), "state is not stable"));
    CHECK(failedWith(runClapSmoke(SPUX_TEST_PLUGIN_NOPARAM), "parameter event"));
    CHECK(failedWith(runClapSmoke(SPUX_TEST_PLUGIN_BADRANGE), "default outside the range"));
}

TEST_CASE("the unstable-state check can be switched off") {
    SmokeOptions o;
    o.stateRoundTrip = false;
    CHECK(runClapSmoke(SPUX_TEST_PLUGIN_STATE, o).ok());
}
