// spux_params: the X-macro expansion, table lookups and checks, the laws and host mapping, value text,
// sanitizing, the atomic store and the edit queue with its gesture reserve.
#include <doctest.h>

#include <cmath>
#include <cstring>
#include <string>
#include <thread>
#include <type_traits>

#include "params/TestParams.hpp"
#include "spux/params/EditQueue.hpp"
#include "spux/params/ParamStore.hpp"
#include "spux/params/ParamText.hpp"

using namespace spux::params;
using testparams::kTable;

TEST_CASE("the X-macro expands to the table, the ids, the count and the smoothers") {
    static_assert(testparams::kCount == 8);
    static_assert(kTable.size() == testparams::kCount);
    static_assert(kTable.valid());
    static_assert(testparams::P_quality == 1002 && testparams::P_out_gain == 8901);
    static_assert(kTable.indexOf(testparams::P_osc_a_level) == 4);
    static_assert(kTable.indexOfKey("env_amp_delay") == 5);
    static_assert(kTable.indexOf(1004) == kNoParam && kTable.indexOfKey("nope") == kNoParam);
    static_assert(kTable.find(1003) != nullptr && kTable.find(1003)->kind == ParamKind::Bool);

    const ParamInfo& q = kTable[0];
    CHECK(std::string(q.key) == "quality");
    CHECK(std::string(q.name) == "Quality");
    CHECK(std::string(q.module) == "Global");
    CHECK(q.flags == (PF_AUTO | PF_STEPPED));
    CHECK(q.def == 1.0);
    CHECK(kTable.findByKey("width")->id == 8902);

    using S = testparams::Smoothers;
    static_assert(std::is_same_v<decltype(S::osc_a_phase_motion_rate), spux::Smoothed<spux::OnePole<0.02>, float>>);
    static_assert(std::is_same_v<decltype(S::osc_a_level), spux::Smoothed<spux::LinearRamp<>, float>>);
    static_assert(std::is_same_v<decltype(S::quality), spux::Smoothed<spux::NoSmoothing, float>>);
    S s;
    s.out_gain.prepare(1000.0); // 200 dB/s at 1 kHz: 0.2 dB a sample
    s.out_gain.reset(0.0f);
    s.out_gain.setTarget(1.0f);
    CHECK(s.out_gain.next() == doctest::Approx(0.2f));
}

TEST_CASE("valid() rejects broken tables") {
    constexpr ParamInfo unsorted[] = {{2, "b"}, {1, "a"}};
    static_assert(!ParamTable(unsorted).valid());
    constexpr ParamInfo duplicateKey[] = {{1, "a"}, {2, "a"}};
    static_assert(!ParamTable(duplicateKey).valid());
    constexpr ParamInfo badDefault[] = {{1, "a", "A", ParamKind::Float, 0.0, 1.0, ParamLaw::Linear, 2.0}};
    static_assert(!ParamTable(badDefault).valid());
    constexpr ParamInfo badLog[] = {{1, "a", "A", ParamKind::Float, 0.0, 1.0, ParamLaw::Log, 0.5}};
    static_assert(!ParamTable(badLog).valid());
    constexpr ParamInfo badLabels[] = {{1, "a", "A", ParamKind::Enum, 0.0, 2.0, ParamLaw::Linear, 0.0, 0, "x|y"}};
    static_assert(!ParamTable(badLabels).valid());
    constexpr ParamInfo good[] = {{1, "a", "A", ParamKind::Enum, 0.0, 1.0, ParamLaw::Linear, 0.0, 0, "x|y"}, {7, "b"}};
    static_assert(ParamTable(good).valid());
}

TEST_CASE("laws: linear, log, log with zero, dB") {
    const ParamInfo& rate = kTable[3];  // Log 0.01 .. 2
    const ParamInfo& level = kTable[4]; // Db -24 .. 6
    const ParamInfo& delay = kTable[5]; // LogZero 0 .. 60
    const ParamInfo& width = kTable[7]; // Linear 0 .. 1

    CHECK(toNormalized(rate, 0.1) == doctest::Approx(std::log(10.0) / std::log(200.0)));
    CHECK(fromNormalized(rate, 0.0) == doctest::Approx(0.01));
    CHECK(fromNormalized(rate, 1.0) == doctest::Approx(2.0));
    CHECK(toNormalized(level, -9.0) == doctest::Approx(0.5));
    CHECK(toNormalized(width, 0.25) == 0.25);
    CHECK(toNormalized(width, 7.0) == 1.0); // clamped

    CHECK(toNormalized(delay, 0.0) == 0.0);
    CHECK(toNormalized(delay, 0.005) == doctest::Approx(0.005)); // the linear knee
    CHECK(toNormalized(delay, 0.01) == doctest::Approx(0.01));
    CHECK(toNormalized(delay, 60.0) == doctest::Approx(1.0));
    CHECK(fromNormalized(delay, 0.0) == 0.0);

    for (const ParamInfo& p : kTable)
        for (int i = 0; i <= 100; ++i) {
            const double n = i / 100.0;
            CHECK(toNormalized(p, fromNormalized(p, n)) == doctest::Approx(n).epsilon(1e-9));
        }
}

TEST_CASE("host values: normalized for log and dB floats, plain otherwise or with PF_HOST_PLAIN") {
    CHECK(!hostIsNormalized(kTable[0])); // Enum
    CHECK(!hostIsNormalized(kTable[2])); // Int
    CHECK(hostIsNormalized(kTable[3]));  // Log
    CHECK(hostIsNormalized(kTable[4]));  // Db
    CHECK(hostIsNormalized(kTable[5]));  // LogZero
    CHECK(!hostIsNormalized(kTable[6])); // Db with PF_HOST_PLAIN
    CHECK(!hostIsNormalized(kTable[7])); // Linear
    CHECK(hostMin(kTable[4]) == 0.0);
    CHECK(hostMax(kTable[4]) == 1.0);
    CHECK(hostDefault(kTable[4]) == doctest::Approx(24.0 / 30.0));
    CHECK(hostMin(kTable[6]) == -96.0);
    CHECK(hostDefault(kTable[6]) == 0.0);
    CHECK(plainToHost(kTable[6], -3.5) == -3.5);
    CHECK(hostToPlain(kTable[4], 0.5) == doctest::Approx(-9.0));
    CHECK(hostToPlain(kTable[7], 1.5) == 1.0);
}

TEST_CASE("sanitizing: non-finite to default, clamped, stepped kinds rounded") {
    CHECK(sanitizePlain(kTable[0], NAN) == 1.0);
    CHECK(sanitizePlain(kTable[0], 1.6) == 2.0);
    CHECK(sanitizePlain(kTable[0], 9.0) == 2.0);
    CHECK(sanitizePlain(kTable[1], 0.4) == 0.0);
    CHECK(sanitizePlain(kTable[2], -3.5) == -4.0);
    CHECK(sanitizePlain(kTable[7], 0.3) == 0.3);
    CHECK(sanitizePlain(kTable[7], INFINITY) == 0.5);
}

TEST_CASE("value text and parsing") {
    char buf[64];
    auto text = [&](std::size_t i, double plain) {
        formatValue(kTable[i], plain, buf, sizeof buf);
        return std::string(buf);
    };
    CHECK(text(0, 2) == "High");
    CHECK(text(1, 1) == "on");
    CHECK(text(1, 0) == "off");
    CHECK(text(2, -7) == "-7");
    CHECK(text(4, -3.26) == "-3.3 dB");
    CHECK(text(6, -96) == "-inf dB");
    CHECK(text(7, 0.125) == "0.125");

    double v = 0;
    CHECK((parseValue(kTable[0], "Normal", v) && v == 1.0));
    CHECK((parseValue(kTable[0], "2", v) && v == 2.0));
    CHECK((parseValue(kTable[1], "on", v) && v == 1.0));
    CHECK((parseValue(kTable[6], "-inf", v) && v == -96.0));
    CHECK((parseValue(kTable[4], "-6 dB", v) && v == -6.0));
    CHECK((parseValue(kTable[7], "3", v) && v == 1.0)); // clamped
    CHECK(!parseValue(kTable[7], "wide", v));
    CHECK(!parseValue(kTable[7], "nan", v));
    CHECK(!parseValue(kTable[7], nullptr, v));
    CHECK(enumCount(kTable[0]) == 3);
    CHECK(enumLabel(kTable[0], 1) == "Normal");
    CHECK(enumLabel(kTable[0], 3).empty());
}

TEST_CASE("the store holds host values, starts at the defaults and counts writes") {
    ParamStore<testparams::kCount> store(kTable);
    CHECK(store.host(4) == doctest::Approx(0.8)); // -0 dB of -24..6, normalized
    CHECK(store.plain(4) == doctest::Approx(0.0));
    CHECK(store.plain(0) == 1.0);
    const auto v0 = store.version();
    store.setPlain(4, -9.0);
    CHECK(store.host(4) == doctest::Approx(0.5));
    CHECK(store.normalized(4) == doctest::Approx(0.5));
    store.setPlain(0, 1.7); // rounded
    CHECK(store.host(0) == 2.0);
    store.setPlain(7, NAN); // default
    CHECK(store.host(7) == 0.5);
    store.setHost(6, -12.0);
    CHECK(store.plain(6) == -12.0);
    CHECK(store.version() == v0 + 4);
    store.resetToDefaults();
    CHECK(store.plain(4) == doctest::Approx(0.0));
    CHECK(store.plain(6) == 0.0);
}

TEST_CASE("edit queue: values leave the gesture reserve free; begins keep a slot for their end") {
    BasicEditQueue<16, 4> q;
    CHECK(q.freeSlots() == 16);
    CHECK(q.pushBegin(3));
    int values = 0;
    while (q.pushValue(3, 0.5))
        ++values;
    CHECK(values == 11); // 16 - 1 begin - 4 reserved
    CHECK(q.freeSlots() == 4);
    CHECK(q.pushBegin(5)); // 4 free, a begin needs 2
    CHECK(q.pushEnd(5));
    CHECK(q.pushEnd(3));
    CHECK(q.freeSlots() == 1);
    CHECK(!q.pushBegin(6)); // no room for its end
    CHECK(q.pushEnd(7));    // an end takes the last slot
    CHECK(!q.pushEnd(8));

    ParamEdit e;
    CHECK((q.pop(e) && e.kind == ParamEdit::Kind::Begin && e.index == 3));
    CHECK((q.pop(e) && e.kind == ParamEdit::Kind::Value && e.host == 0.5));
    int n = 2;
    while (q.pop(e))
        ++n;
    CHECK(n == 16);
    CHECK(q.empty());
}

TEST_CASE("edit queue: one producer and one consumer thread keep order") {
    EditQueue q;
    constexpr std::uint32_t kN = 200000;
    std::thread producer([&] {
        for (std::uint32_t i = 0; i < kN;)
            if (q.tryPush({ParamEdit::Kind::Value, i, static_cast<double>(i)}))
                ++i;
    });
    std::uint32_t next = 0;
    bool ordered = true;
    while (next < kN) {
        ParamEdit e;
        if (q.pop(e)) {
            ordered = ordered && e.index == next && e.host == static_cast<double>(next);
            ++next;
        }
    }
    producer.join();
    CHECK(ordered);
    CHECK(q.empty());
}
