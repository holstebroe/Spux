// spux_clap: the CRTP glue offers exactly the extensions a plugin class implements, forwards every call,
// keeps exceptions out of the host, lets the plugin answer extensions itself; the helpers fill parameter
// info, drain the edit queue into host events and read and write whole streams.
#include <doctest.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "params/TestParams.hpp"
#include "spux/clap/ClapHelpers.hpp"
#include "spux/clap/ClapPlugin.hpp"
#include "spux/params/ParamStore.hpp"

namespace sc = spux::clap;
using namespace spux::params;
using testparams::kTable;

namespace {

const clap_host_t kHost = {
    CLAP_VERSION,
    nullptr,
    "test",
    "spux",
    "",
    "1",
    [](const clap_host_t*, const char*) -> const void* { return nullptr; },
    [](const clap_host_t*) {},
    [](const clap_host_t*) {},
    [](const clap_host_t*) {},
};

const char* const kFeatures[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, nullptr};
const clap_plugin_descriptor_t kDesc = {CLAP_VERSION, "org.spux.glue-test", "Glue", "spux", "", "", "", "1", "",
                                        kFeatures};

// The smallest plugin: lifecycle defaults only, no extensions.
struct Bare : sc::Plugin<Bare> {
    explicit Bare(const clap_host_t* h) : Plugin(&kDesc, h) {}
    static const clap_plugin_descriptor_t& descriptor() { return kDesc; }
};

// Params, state, audio ports, gui (required part only) and timer.
struct Full : sc::Plugin<Full> {
    ParamStore<testparams::kCount> store{kTable};
    EditQueue edits;
    int timers = 0, activations = 0;
    bool guiOpen = false;
    static int destroyed;

    explicit Full(const clap_host_t* h) : Plugin(&kDesc, h) {}
    ~Full() { ++destroyed; }
    static const clap_plugin_descriptor_t& descriptor() { return kDesc; }

    bool activate(double, std::uint32_t, std::uint32_t) {
        ++activations;
        return true;
    }
    clap_process_status process(const clap_process_t*) { throw std::runtime_error("never into the host"); }

    std::uint32_t paramsCount() const { return static_cast<std::uint32_t>(kTable.size()); }
    bool paramsInfo(std::uint32_t i, clap_param_info_t* info) const {
        if (i >= kTable.size())
            return false;
        sc::fillParamInfo(kTable[i], *info);
        return true;
    }
    bool paramsValue(clap_id id, double* v) const {
        const auto i = kTable.indexOf(id);
        if (i == kNoParam)
            return false;
        *v = store.host(i);
        return true;
    }
    bool paramsValueToText(clap_id id, double v, char* buf, std::uint32_t cap) const {
        const ParamInfo* p = kTable.find(id);
        return p != nullptr && sc::hostValueToText(*p, v, buf, cap);
    }
    bool paramsTextToValue(clap_id id, const char* text, double* v) const {
        const ParamInfo* p = kTable.find(id);
        return p != nullptr && sc::textToHostValue(*p, text, v);
    }
    void paramsFlush(const clap_input_events_t* in, const clap_output_events_t* out) {
        for (std::uint32_t i = 0, n = in ? in->size(in) : 0; i < n; ++i)
            if (const auto* ev = sc::asParamValue(in->get(in, i)))
                if (const auto k = kTable.indexOf(ev->param_id); k != kNoParam)
                    store.setHost(k, ev->value);
        sc::drainEdits(edits, kTable, out);
    }

    bool stateSave(const clap_ostream_t* s) { return sc::writeAll(s, std::vector<std::uint8_t>{1, 2, 3}); }
    bool stateLoad(const clap_istream_t* s) {
        std::vector<std::uint8_t> b;
        return sc::readAll(s, b, 8) && b.size() == 3;
    }

    std::uint32_t audioPortsCount(bool) const { return 1; }
    bool audioPortsGet(std::uint32_t i, bool, clap_audio_port_info_t* info) const {
        if (i != 0)
            return false;
        info->channel_count = 2;
        return true;
    }

    bool guiCreate(const char*, bool) { return guiOpen = true; }
    void guiDestroy() { guiOpen = false; }
    bool guiSetParent(const clap_window_t*) { return true; }
    bool guiGetSize(std::uint32_t* w, std::uint32_t* h) const {
        *w = 640;
        *h = 480;
        return true;
    }
    bool guiShow() { return true; }
    bool guiHide() { return true; }

    void onTimer(clap_id) { ++timers; }
};
int Full::destroyed = 0;

// Answers one extension itself and hides the gui the default would offer.
struct Custom : sc::Plugin<Custom> {
    explicit Custom(const clap_host_t* h) : Plugin(&kDesc, h) {}
    static inline const int kMarker = 0;
    std::uint32_t audioPortsCount(bool) const { return 0; }
    bool audioPortsGet(std::uint32_t, bool, clap_audio_port_info_t*) const { return false; }
    const void* getExtension(const char* id) {
        if (std::strcmp(id, "org.spux.custom") == 0)
            return &kMarker;
        return defaultExtension(id);
    }
};

template <class T> const T* ext(const clap_plugin_t* p, const char* id) {
    return static_cast<const T*>(p->get_extension(p, id));
}

struct Events {
    std::vector<std::uint16_t> types;
    std::vector<clap_id> ids;
    std::vector<double> values;
    clap_output_events_t out{this, [](const clap_output_events_t* o, const clap_event_header_t* h) -> bool {
                                 auto* self = static_cast<Events*>(o->ctx);
                                 self->types.push_back(h->type);
                                 if (h->type == CLAP_EVENT_PARAM_VALUE) {
                                     const auto* v = reinterpret_cast<const clap_event_param_value_t*>(h);
                                     self->ids.push_back(v->param_id);
                                     self->values.push_back(v->value);
                                 } else {
                                     self->ids.push_back(
                                         reinterpret_cast<const clap_event_param_gesture_t*>(h)->param_id);
                                     self->values.push_back(0.0);
                                 }
                                 return true;
                             }};
};

} // namespace

TEST_CASE("a bare plugin: defaults for the lifecycle, no extensions") {
    auto* b = new Bare(&kHost);
    const clap_plugin_t* p = b->clapPlugin();
    CHECK(p->desc == &kDesc);
    CHECK(p->init(p));
    CHECK(p->activate(p, 48000, 32, 512));
    CHECK(p->start_processing(p));
    clap_process_t pr{};
    CHECK(p->process(p, &pr) == CLAP_PROCESS_CONTINUE);
    CHECK(p->process(p, nullptr) == CLAP_PROCESS_ERROR);
    for (const char* id : {CLAP_EXT_PARAMS, CLAP_EXT_STATE, CLAP_EXT_AUDIO_PORTS, CLAP_EXT_NOTE_PORTS, CLAP_EXT_GUI,
                           CLAP_EXT_TIMER_SUPPORT})
        CHECK(p->get_extension(p, id) == nullptr);
    CHECK(p->get_extension(p, nullptr) == nullptr);
    p->destroy(p);
}

TEST_CASE("the extensions a plugin implements, forwarded") {
    Full::destroyed = 0;
    auto* f = new Full(&kHost);
    const clap_plugin_t* p = f->clapPlugin();
    CHECK(p->activate(p, 44100, 1, 64));
    CHECK(f->activations == 1);

    clap_process_t pr{};
    CHECK(p->process(p, &pr) == CLAP_PROCESS_ERROR); // the exception stopped at the glue

    const auto* params = ext<clap_plugin_params_t>(p, CLAP_EXT_PARAMS);
    REQUIRE(params != nullptr);
    CHECK(params->count(p) == 8);
    clap_param_info_t info;
    REQUIRE(params->get_info(p, 0, &info));
    CHECK(info.id == 1002);
    CHECK(std::string(info.name) == "Quality");
    CHECK(std::string(info.module) == "Global");
    CHECK(info.flags ==
          static_cast<std::uint32_t>(CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM));
    CHECK((info.min_value == 0 && info.max_value == 2 && info.default_value == 1));
    REQUIRE(params->get_info(p, 4, &info)); // dB: the host sees 0..1
    CHECK((info.min_value == 0 && info.max_value == 1));
    CHECK(info.default_value == doctest::Approx(0.8));
    CHECK(info.flags == static_cast<std::uint32_t>(CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE));
    CHECK(!params->get_info(p, 8, &info));
    CHECK(!params->get_info(p, 0, nullptr));

    char buf[32];
    CHECK((params->value_to_text(p, 2115, 0.5, buf, sizeof buf) && std::string(buf) == "-9.0 dB"));
    double v = 0;
    CHECK((params->text_to_value(p, 2115, "-24", &v) && v == 0.0));
    CHECK(!params->text_to_value(p, 4242, "1", &v));

    clap_event_param_value_t ev{};
    ev.header = {sizeof(ev), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
    ev.param_id = 8902;
    ev.value = 0.75;
    const clap_input_events_t in{
        &ev, [](const clap_input_events_t*) -> std::uint32_t { return 1; },
        [](const clap_input_events_t* l, std::uint32_t) { return static_cast<const clap_event_header_t*>(l->ctx); }};
    f->edits.pushBegin(4);
    f->edits.pushValue(4, 0.25);
    f->edits.pushEnd(4);
    Events out;
    params->flush(p, &in, &out.out);
    CHECK((params->get_value(p, 8902, &v) && v == 0.75));
    REQUIRE(out.types.size() == 3);
    CHECK(out.types[0] == CLAP_EVENT_PARAM_GESTURE_BEGIN);
    CHECK((out.types[1] == CLAP_EVENT_PARAM_VALUE && out.ids[1] == 2115 && out.values[1] == 0.25));
    CHECK((out.types[2] == CLAP_EVENT_PARAM_GESTURE_END && out.ids[2] == 2115));
    CHECK(f->edits.empty());

    const auto* state = ext<clap_plugin_state_t>(p, CLAP_EXT_STATE);
    REQUIRE(state != nullptr);
    std::vector<std::uint8_t> saved;
    const clap_ostream_t os{&saved, [](const clap_ostream_t* s, const void* d, std::uint64_t n) -> std::int64_t {
                                auto* b = static_cast<std::vector<std::uint8_t>*>(s->ctx);
                                const auto* c = static_cast<const std::uint8_t*>(d);
                                b->insert(b->end(), c, c + (n > 2 ? 2 : n)); // in parts
                                return static_cast<std::int64_t>(n > 2 ? 2 : n);
                            }};
    CHECK(state->save(p, &os));
    CHECK(saved == std::vector<std::uint8_t>{1, 2, 3});
    struct Src {
        std::vector<std::uint8_t> d;
        std::size_t pos = 0;
    } src{saved};
    const clap_istream_t is{&src, [](const clap_istream_t* s, void* b, std::uint64_t n) -> std::int64_t {
                                auto* r = static_cast<Src*>(s->ctx);
                                const std::size_t k = std::min<std::size_t>(n, r->d.size() - r->pos);
                                std::memcpy(b, r->d.data() + r->pos, k);
                                r->pos += k;
                                return static_cast<std::int64_t>(k);
                            }};
    CHECK(state->load(p, &is));
    CHECK(!state->load(p, nullptr));

    const auto* ports = ext<clap_plugin_audio_ports_t>(p, CLAP_EXT_AUDIO_PORTS);
    REQUIRE(ports != nullptr);
    clap_audio_port_info_t pi{};
    CHECK((ports->count(p, true) == 1 && ports->get(p, 0, true, &pi) && pi.channel_count == 2));
    CHECK(ext<clap_plugin_note_ports_t>(p, CLAP_EXT_NOTE_PORTS) == nullptr);

    const auto* gui = ext<clap_plugin_gui_t>(p, CLAP_EXT_GUI);
    REQUIRE(gui != nullptr);
    if (sc::kHasPlatformWindowApi) {
        CHECK(gui->is_api_supported(p, sc::platformWindowApi(), false));
        CHECK(!gui->is_api_supported(p, sc::platformWindowApi(), true));
        const char* api = nullptr;
        bool floating = true;
        CHECK((gui->get_preferred_api(p, &api, &floating) && std::strcmp(api, sc::platformWindowApi()) == 0 &&
               !floating));
    }
    CHECK(!gui->is_api_supported(p, "no-such-api", false));
    CHECK((gui->create(p, CLAP_WINDOW_API_X11, false) && f->guiOpen));
    std::uint32_t w = 1, h = 1;
    CHECK((gui->adjust_size(p, &w, &h) && w == 640 && h == 480)); // default: fixed size
    CHECK(gui->set_size(p, 640, 480));
    CHECK(!gui->set_size(p, 641, 480));
    CHECK(!gui->can_resize(p));
    CHECK(!gui->set_scale(p, 2.0));
    gui->suggest_title(p, "x");
    gui->destroy(p);
    CHECK(!f->guiOpen);

    const auto* timer = ext<clap_plugin_timer_support_t>(p, CLAP_EXT_TIMER_SUPPORT);
    REQUIRE(timer != nullptr);
    timer->on_timer(p, 3);
    CHECK(f->timers == 1);

    // Host calls without host extensions are no-ops.
    clap_id tid = 0;
    CHECK(!f->registerTimer(30, &tid));
    f->requestParamsFlush();
    f->rescanParams(CLAP_PARAM_RESCAN_VALUES);
    f->markStateDirty();

    p->destroy(p);
    CHECK(Full::destroyed == 1);
}

TEST_CASE("a plugin answers extensions itself and falls back to the default") {
    Custom c(&kHost);
    const clap_plugin_t* p = c.clapPlugin();
    CHECK(p->get_extension(p, "org.spux.custom") == &Custom::kMarker);
    CHECK(p->get_extension(p, CLAP_EXT_AUDIO_PORTS) != nullptr);
    CHECK(p->get_extension(p, CLAP_EXT_PARAMS) == nullptr);
}

TEST_CASE("the single-plugin factory") {
    const clap_plugin_factory_t& f = sc::SingleFactory<Full>::factory;
    CHECK(f.get_plugin_count(&f) == 1);
    CHECK(f.get_plugin_descriptor(&f, 0) == &kDesc);
    CHECK(f.get_plugin_descriptor(&f, 1) == nullptr);
    CHECK(f.create_plugin(&f, &kHost, "other.id") == nullptr);
    CHECK(f.create_plugin(&f, nullptr, kDesc.id) == nullptr);
    const clap_plugin_t* p = f.create_plugin(&f, &kHost, kDesc.id);
    REQUIRE(p != nullptr);
    CHECK(p->init(p));
    p->destroy(p);
}

TEST_CASE("readAll refuses streams larger than the limit") {
    struct Endless {
        static std::int64_t read(const clap_istream_t*, void* b, std::uint64_t n) {
            std::memset(b, 0, n);
            return static_cast<std::int64_t>(n);
        }
    };
    const clap_istream_t is{nullptr, Endless::read};
    std::vector<std::uint8_t> out;
    CHECK(!sc::readAll(&is, out, 10000));
    CHECK(!sc::readAll(nullptr, out));
    CHECK(!sc::writeAll(nullptr, out));
}
