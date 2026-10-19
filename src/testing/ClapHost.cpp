#include "spux/testing/ClapHost.hpp"

#include <clap/ext/log.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#ifdef SPUX_HAS_X11
#include <X11/Xlib.h>
#endif

namespace spux::inline v1::testing {
namespace {

// --- Loading ----------------------------------------------------------------------------------

struct Library {
    void* handle = nullptr;
    bool open(const std::string& path, std::string& error) {
#ifdef _WIN32
        handle = reinterpret_cast<void*>(LoadLibraryA(path.c_str()));
        if (!handle)
            error = "LoadLibrary failed (error " + std::to_string(GetLastError()) + ")";
#else
        handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle)
            error = dlerror();
#endif
        return handle != nullptr;
    }
    void* symbol(const char* name) const {
#ifdef _WIN32
        return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
        return dlsym(handle, name);
#endif
    }
    ~Library() {
        if (!handle)
            return;
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
    }
};

// --- The host ---------------------------------------------------------------------------------

struct Host {
    HostActivity activity;
    SmokeReport* report = nullptr;
    clap_host_t host{};
    clap_id nextTimer = 1;

    static Host& of(const clap_host_t* h) { return *static_cast<Host*>(h->host_data); }

    static const void* extension(const clap_host_t*, const char* id) {
        static const clap_host_timer_support_t timer = {[](const clap_host_t* h, uint32_t, clap_id* out) {
                                                            Host& s = of(h);
                                                            *out = s.nextTimer++;
                                                            s.activity.timerIds.push_back(*out);
                                                            ++s.activity.timerRegistrations;
                                                            return true;
                                                        },
                                                        [](const clap_host_t* h, clap_id id) {
                                                            Host& s = of(h);
                                                            auto& v = s.activity.timerIds;
                                                            v.erase(std::remove(v.begin(), v.end(), id), v.end());
                                                            ++s.activity.timerUnregistrations;
                                                            return true;
                                                        }};
        static const clap_host_params_t params = {
            [](const clap_host_t* h, clap_param_rescan_flags) { ++of(h).activity.paramRescans; },
            [](const clap_host_t*, clap_id, clap_param_clear_flags) {},
            [](const clap_host_t* h) { ++of(h).activity.paramFlushRequests; }};
        static const clap_host_state_t state = {[](const clap_host_t* h) { ++of(h).activity.stateDirtyMarks; }};
        static const clap_host_gui_t gui = {[](const clap_host_t*) {},
                                            [](const clap_host_t* h, uint32_t, uint32_t) {
                                                ++of(h).activity.resizeRequests;
                                                return true;
                                            },
                                            [](const clap_host_t*) { return true; },
                                            [](const clap_host_t*) { return true; }, [](const clap_host_t*, bool) {}};
        static const clap_host_log_t log = {[](const clap_host_t* h, clap_log_severity severity, const char* msg) {
            Host& s = of(h);
            if (s.report && severity >= CLAP_LOG_WARNING)
                s.report->notes.push_back(std::string("plugin log: ") + (msg ? msg : ""));
        }};
        if (!std::strcmp(id, CLAP_EXT_TIMER_SUPPORT))
            return &timer;
        if (!std::strcmp(id, CLAP_EXT_PARAMS))
            return &params;
        if (!std::strcmp(id, CLAP_EXT_STATE))
            return &state;
        if (!std::strcmp(id, CLAP_EXT_GUI))
            return &gui;
        if (!std::strcmp(id, CLAP_EXT_LOG))
            return &log;
        return nullptr;
    }

    Host() {
        host.clap_version = CLAP_VERSION;
        host.host_data = this;
        host.name = "spux test host";
        host.vendor = "spux";
        host.url = "";
        host.version = "1";
        host.get_extension = &Host::extension;
        host.request_restart = [](const clap_host_t* h) { ++of(h).activity.restartRequests; };
        host.request_process = [](const clap_host_t*) {};
        host.request_callback = [](const clap_host_t*) {};
    }
};

// --- Events -----------------------------------------------------------------------------------

struct RawEvent {
    alignas(16) std::uint8_t bytes[128];
};
static_assert(sizeof(clap_event_note_t) <= 128 && sizeof(clap_event_param_value_t) <= 128);

struct EventList {
    std::vector<RawEvent> events;
    template <typename T> void add(const T& ev) {
        RawEvent r{};
        std::memcpy(r.bytes, &ev, sizeof(T));
        events.push_back(r);
    }
    void clear() { events.clear(); }
};

std::uint32_t evSize(const clap_input_events_t* l) {
    return static_cast<std::uint32_t>(static_cast<EventList*>(l->ctx)->events.size());
}
const clap_event_header_t* evGet(const clap_input_events_t* l, std::uint32_t i) {
    return reinterpret_cast<const clap_event_header_t*>(static_cast<EventList*>(l->ctx)->events[i].bytes);
}
bool evPush(const clap_output_events_t* l, const clap_event_header_t* h) {
    auto* list = static_cast<EventList*>(l->ctx);
    RawEvent r{};
    std::memcpy(r.bytes, h, std::min<std::size_t>(h->size, sizeof(r.bytes)));
    list->events.push_back(r);
    return true;
}

clap_event_note_t noteEvent(const SmokeNote& n, std::uint32_t time) {
    clap_event_note_t ev{};
    ev.header = {sizeof(ev), time, CLAP_CORE_EVENT_SPACE_ID,
                 static_cast<std::uint16_t>(n.on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF), 0};
    ev.note_id = -1;
    ev.port_index = 0;
    ev.channel = 0;
    ev.key = static_cast<std::int16_t>(n.key);
    ev.velocity = n.on ? n.velocity : 0.0;
    return ev;
}

// --- Streams ----------------------------------------------------------------------------------

struct InStream {
    const std::vector<std::uint8_t>* data;
    std::size_t pos = 0;
};

bool saveState(const clap_plugin_t* p, const clap_plugin_state_t* st, std::vector<std::uint8_t>& out) {
    out.clear();
    clap_ostream_t os{&out, [](const clap_ostream_t* s, const void* b, std::uint64_t n) -> std::int64_t {
                          auto* v = static_cast<std::vector<std::uint8_t>*>(s->ctx);
                          v->insert(v->end(), static_cast<const std::uint8_t*>(b),
                                    static_cast<const std::uint8_t*>(b) + n);
                          return static_cast<std::int64_t>(n);
                      }};
    return st->save(p, &os);
}

bool loadState(const clap_plugin_t* p, const clap_plugin_state_t* st, const std::vector<std::uint8_t>& in) {
    InStream rd{&in, 0};
    clap_istream_t is{&rd, [](const clap_istream_t* s, void* b, std::uint64_t n) -> std::int64_t {
                          auto* r = static_cast<InStream*>(s->ctx);
                          const std::size_t k =
                              std::min<std::size_t>(static_cast<std::size_t>(n), r->data->size() - r->pos);
                          std::memcpy(b, r->data->data() + r->pos, k);
                          r->pos += k;
                          return static_cast<std::int64_t>(k);
                      }};
    return st->load(p, &is);
}

std::string fmt(const char* f, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return buf;
}

// --- Checks -----------------------------------------------------------------------------------

struct ParamFacts {
    clap_id id;
    clap_param_info_t info;
};

void checkParams(const SmokeSession& s, const SmokeOptions& o, SmokeReport& r, std::vector<ParamFacts>& facts) {
    const auto* params = s.extension<clap_plugin_params_t>(CLAP_EXT_PARAMS);
    if (!params) {
        r.notes.push_back("no params extension");
        r.check(!o.paramCount || *o.paramCount == 0, "params extension missing but a parameter count was expected");
        return;
    }
    const std::uint32_t n = params->count(s.plugin);
    if (o.paramCount)
        r.check(n == *o.paramCount, fmt("parameter count %u, expected %u", n, *o.paramCount));
    for (std::uint32_t i = 0; i < n; ++i) {
        clap_param_info_t info{};
        if (!params->get_info(s.plugin, i, &info)) {
            r.check(false, fmt("param %u: get_info failed", i));
            continue;
        }
        r.check(info.min_value <= info.max_value, fmt("param %u: min above max", i));
        r.check(info.default_value >= info.min_value && info.default_value <= info.max_value,
                fmt("param %u (%s): default outside the range", i, info.name));
        r.check(info.name[0] != '\0', fmt("param %u: empty name", i));
        for (const ParamFacts& f : facts)
            r.check(f.id != info.id, fmt("param %u: id %u is used twice", i, info.id));
        facts.push_back({info.id, info});
        double v = 0.0;
        r.check(params->get_value(s.plugin, info.id, &v), fmt("param %u (%s): get_value failed", i, info.name));
        r.check(v >= info.min_value && v <= info.max_value,
                fmt("param %u (%s): value %g outside the range", i, info.name, v));
        if (o.textRoundTrip) {
            char text[128];
            if (params->value_to_text(s.plugin, info.id, v, text, sizeof(text))) {
                double back = 0.0;
                const double tol = 0.01 * (info.max_value - info.min_value) + 1e-9;
                const bool parsed = params->text_to_value(s.plugin, info.id, text, &back);
                r.check(parsed && std::fabs(back - v) <= tol,
                        fmt("param %u (%s): text '%s' of %g reads back as %g", i, info.name, text, v, back));
            }
        }
    }
}

void checkPorts(const SmokeSession& s, SmokeReport& r, std::uint32_t& audioIn, std::uint32_t& audioOut, bool& noteIn,
                std::vector<std::uint32_t>& inChannels, std::vector<std::uint32_t>& outChannels) {
    if (const auto* ap = s.extension<clap_plugin_audio_ports_t>(CLAP_EXT_AUDIO_PORTS)) {
        audioIn = ap->count(s.plugin, true);
        audioOut = ap->count(s.plugin, false);
        for (int dir = 0; dir < 2; ++dir) {
            const bool input = dir == 0;
            for (std::uint32_t i = 0; i < (input ? audioIn : audioOut); ++i) {
                clap_audio_port_info_t info{};
                if (!ap->get(s.plugin, i, input, &info)) {
                    r.check(false, fmt("audio %s port %u: get failed", input ? "input" : "output", i));
                    (input ? inChannels : outChannels).push_back(2);
                    continue;
                }
                r.check(info.channel_count > 0, fmt("audio %s port %u: no channels", input ? "input" : "output", i));
                (input ? inChannels : outChannels).push_back(std::max<std::uint32_t>(info.channel_count, 1));
            }
        }
    } else {
        r.notes.push_back("no audio-ports extension");
    }
    if (const auto* np = s.extension<clap_plugin_note_ports_t>(CLAP_EXT_NOTE_PORTS)) {
        noteIn = np->count(s.plugin, true) > 0;
        for (int dir = 0; dir < 2; ++dir)
            for (std::uint32_t i = 0; i < np->count(s.plugin, dir == 0); ++i) {
                clap_note_port_info_t info{};
                r.check(np->get(s.plugin, i, dir == 0, &info), fmt("note port %u: get failed", i));
            }
    }
}

#ifdef SPUX_HAS_X11
void checkGui(const SmokeSession& s, Host& host, SmokeReport& r) {
    const auto* gui = s.extension<clap_plugin_gui_t>(CLAP_EXT_GUI);
    if (!gui) {
        r.notes.push_back("no gui extension; GUI part skipped");
        return;
    }
    if (!std::getenv("DISPLAY")) {
        r.notes.push_back("no DISPLAY; GUI part skipped");
        return;
    }
    if (!gui->is_api_supported(s.plugin, CLAP_WINDOW_API_X11, false)) {
        r.notes.push_back("plugin has no X11 GUI; GUI part skipped");
        return;
    }
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        r.notes.push_back("cannot open the display; GUI part skipped");
        return;
    }
    r.check(gui->create(s.plugin, CLAP_WINDOW_API_X11, false), "gui create");
    std::uint32_t w = 0, h = 0;
    r.check(gui->get_size(s.plugin, &w, &h) && w > 0 && h > 0, "gui get_size");
    Window parent = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, std::max(w, 1u), std::max(h, 1u), 0, 0, 0);
    XMapWindow(dpy, parent);
    XSync(dpy, False);
    clap_window_t cw{};
    cw.api = CLAP_WINDOW_API_X11;
    cw.x11 = parent;
    r.check(gui->set_parent(s.plugin, &cw), "gui set_parent");
    r.check(gui->show(s.plugin), "gui show");
    // The plugin window must become a child of the host's window.
    unsigned children = 0;
    for (int tries = 0; tries < 20 && children == 0; ++tries) {
        Window root, par, *kids = nullptr;
        unsigned count = 0;
        if (XQueryTree(dpy, parent, &root, &par, &kids, &count))
            children = count;
        if (kids)
            XFree(kids);
        if (children == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    r.check(children >= 1, "the plugin window is not a child of the host window");
    if (const auto* timer = s.extension<clap_plugin_timer_support_t>(CLAP_EXT_TIMER_SUPPORT))
        for (clap_id id : std::vector<clap_id>(host.activity.timerIds))
            for (int i = 0; i < 3; ++i)
                timer->on_timer(s.plugin, id);
    r.check(gui->hide(s.plugin), "gui hide");
    gui->destroy(s.plugin);
    XDestroyWindow(dpy, parent);
    XCloseDisplay(dpy);
}
#endif

} // namespace

std::string SmokeReport::text() const {
    std::string s = fmt("%d checks, %zu failure(s)\n", checks, failures.size());
    for (const std::string& f : failures)
        s += "  FAIL " + f + "\n";
    for (const std::string& n : notes)
        s += "  note " + n + "\n";
    return s;
}

SmokeReport runClapSmoke(const std::string& clapPath, const SmokeOptions& o) {
    SmokeReport r;
    Host host;
    host.report = &r;

    Library lib;
    std::string error;
    if (!lib.open(clapPath, error)) {
        r.check(false, "cannot load " + clapPath + ": " + error);
        return r;
    }
    const auto* entry = static_cast<const clap_plugin_entry_t*>(lib.symbol("clap_entry"));
    r.check(entry != nullptr, "clap_entry is not exported");
    if (!entry)
        return r;
    r.check(clap_version_is_compatible(entry->clap_version), "clap_entry has an incompatible CLAP version");
    if (!entry->init(clapPath.c_str())) {
        r.check(false, "entry init failed");
        return r;
    }
    struct EntryGuard {
        const clap_plugin_entry_t* e;
        ~EntryGuard() { e->deinit(); }
    } entryGuard{entry};

    const auto* factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    r.check(factory != nullptr, "no plugin factory");
    if (!factory)
        return r;
    const std::uint32_t count = factory->get_plugin_count(factory);
    if (o.pluginCount)
        r.check(count == *o.pluginCount, fmt("factory has %u plugins, expected %u", count, *o.pluginCount));
    r.check(count > 0, "the factory has no plugins");
    const clap_plugin_descriptor_t* desc = nullptr;
    for (std::uint32_t i = 0; i < count; ++i) {
        const clap_plugin_descriptor_t* d = factory->get_plugin_descriptor(factory, i);
        r.check(d && d->id && d->name && d->version, fmt("descriptor %u is incomplete", i));
        if (d && d->id && (o.pluginId.empty() ? !desc : o.pluginId == d->id) && !desc)
            desc = d;
    }
    if (!desc) {
        r.check(false,
                o.pluginId.empty() ? "no plugin descriptor" : "plugin id " + o.pluginId + " is not in the factory");
        return r;
    }

    const clap_plugin_t* p = factory->create_plugin(factory, &host.host, desc->id);
    r.check(p != nullptr, "create_plugin failed");
    if (!p)
        return r;
    if (!p->init(p)) {
        r.check(false, "plugin init failed");
        p->destroy(p);
        return r;
    }
    SmokeSession session;
    session.plugin = p;
    session.descriptor = desc;
    session.host = &host.activity;
    session.sampleRate = o.sampleRate;

    std::vector<ParamFacts> facts;
    checkParams(session, o, r, facts);
    std::uint32_t audioIn = 0, audioOut = 0;
    bool noteIn = false;
    std::vector<std::uint32_t> inCh, outCh;
    checkPorts(session, r, audioIn, audioOut, noteIn, inCh, outCh);

    if (!p->activate(p, o.sampleRate, o.minFrames, o.maxFrames)) {
        r.check(false, "activate failed");
        p->destroy(p);
        return r;
    }
    r.check(p->start_processing(p), "start_processing failed");

    // Audio buffers for every port.
    const std::uint32_t frames = o.blockSize;
    std::vector<std::vector<std::vector<float>>> inData(inCh.size()), outData(outCh.size());
    std::vector<std::vector<float*>> inPtr(inCh.size()), outPtr(outCh.size());
    std::vector<clap_audio_buffer_t> inBuf(inCh.size()), outBuf(outCh.size());
    for (std::size_t i = 0; i < inCh.size(); ++i) {
        inData[i].assign(inCh[i], std::vector<float>(frames, 0.f));
        for (auto& c : inData[i])
            inPtr[i].push_back(c.data());
        inBuf[i] = {};
        inBuf[i].data32 = inPtr[i].data();
        inBuf[i].channel_count = inCh[i];
    }
    for (std::size_t i = 0; i < outCh.size(); ++i) {
        outData[i].assign(outCh[i], std::vector<float>(frames, 0.f));
        for (auto& c : outData[i])
            outPtr[i].push_back(c.data());
        outBuf[i] = {};
        outBuf[i].data32 = outPtr[i].data();
        outBuf[i].channel_count = outCh[i];
    }

    SmokeInput input = o.input;
    if (input == SmokeInput::Auto)
        input = (audioIn > 0 && !noteIn) ? SmokeInput::Noise : SmokeInput::Silence;
    std::vector<SmokeNote> notes = o.notes;
    if (notes.empty() && noteIn) {
        const std::uint32_t total = static_cast<std::uint32_t>(o.blocks) * frames;
        for (int key : {60, 64, 67}) {
            notes.push_back({0, true, key, 0.8});
            notes.push_back({total * 6 / 10, false, key, 0.0});
        }
    }
    std::sort(notes.begin(), notes.end(), [](const SmokeNote& a, const SmokeNote& b) { return a.frame < b.frame; });

    // The parameter event: the first writable parameter goes to the end of its range that it is not at.
    const ParamFacts* target = nullptr;
    for (const ParamFacts& f : facts)
        if (!(f.info.flags & (CLAP_PARAM_IS_READONLY | CLAP_PARAM_IS_HIDDEN)) && f.info.min_value < f.info.max_value) {
            target = &f;
            break;
        }
    double targetValue = 0.0;
    if (o.paramEvent && target)
        targetValue =
            target->info.default_value < target->info.max_value ? target->info.max_value : target->info.min_value;

    EventList in, out;
    clap_input_events_t ie{&in, evSize, evGet};
    clap_output_events_t oe{&out, evPush};
    clap_event_transport_t tr{};
    tr.header = {sizeof(tr), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_TRANSPORT, 0};
    tr.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE | CLAP_TRANSPORT_HAS_SECONDS_TIMELINE |
               CLAP_TRANSPORT_IS_PLAYING;
    tr.tempo = o.tempo;
    tr.tsig_num = 4;
    tr.tsig_denom = 4;
    clap_process_t pr{};
    pr.frames_count = frames;
    pr.audio_inputs = inBuf.empty() ? nullptr : inBuf.data();
    pr.audio_inputs_count = static_cast<std::uint32_t>(inBuf.size());
    pr.audio_outputs = outBuf.empty() ? nullptr : outBuf.data();
    pr.audio_outputs_count = static_cast<std::uint32_t>(outBuf.size());
    pr.in_events = &ie;
    pr.out_events = &oe;
    pr.transport = &tr;

    double peak = 0.0;
    bool finite = true, errored = false;
    std::size_t nextNote = 0;
    std::uint32_t noiseState = 0x12345678u;
    for (int b = 0; b < o.blocks; ++b) {
        const std::uint32_t start = static_cast<std::uint32_t>(b) * frames;
        in.clear();
        out.clear();
        if (b == 0 && o.paramEvent && target) {
            clap_event_param_value_t pv{};
            pv.header = {sizeof(pv), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
            pv.param_id = target->id;
            pv.cookie = target->info.cookie;
            pv.note_id = -1;
            pv.port_index = pv.channel = pv.key = -1;
            pv.value = targetValue;
            in.add(pv);
        }
        while (nextNote < notes.size() && notes[nextNote].frame < start + frames) {
            in.add(noteEvent(notes[nextNote], notes[nextNote].frame > start ? notes[nextNote].frame - start : 0));
            ++nextNote;
        }
        for (auto& port : inData)
            for (auto& ch : port)
                for (std::uint32_t i = 0; i < frames; ++i) {
                    if (input == SmokeInput::Impulse)
                        ch[i] = (b == 0 && i == 0) ? 1.f : 0.f;
                    else if (input == SmokeInput::Noise) {
                        noiseState = noiseState * 1664525u + 1013904223u;
                        ch[i] = 0.25f * (static_cast<float>(noiseState >> 8) / 8388608.f - 1.f);
                    }
                }
        tr.song_pos_seconds =
            static_cast<clap_sectime>(static_cast<double>(start) / o.sampleRate * CLAP_SECTIME_FACTOR);
        tr.song_pos_beats = static_cast<clap_beattime>(static_cast<double>(start) / o.sampleRate * o.tempo / 60.0 *
                                                       CLAP_BEATTIME_FACTOR);
        pr.steady_time = start;
        if (p->process(p, &pr) == CLAP_PROCESS_ERROR)
            errored = true;
        for (auto& port : outData)
            for (auto& ch : port)
                for (float v : ch) {
                    if (!std::isfinite(v))
                        finite = false;
                    else
                        peak = std::max(peak, static_cast<double>(std::fabs(v)));
                }
        if (b == 0 && o.paramEvent && target) {
            const auto* params = session.extension<clap_plugin_params_t>(CLAP_EXT_PARAMS);
            double got = 0.0;
            const double tol = 1e-6 * (target->info.max_value - target->info.min_value) + 1e-12;
            r.check(params && params->get_value(p, target->id, &got) && std::fabs(got - targetValue) <= tol,
                    fmt("parameter event: %s was set to %g and reads %g", target->info.name, targetValue, got));
        }
    }
    r.check(!errored, "process returned CLAP_PROCESS_ERROR");
    r.check(finite, "the output has non-finite samples");
    r.check(peak < o.maxPeak, fmt("output peak %g is not below %g", peak, o.maxPeak));
    r.check(peak >= o.minPeak, fmt("output peak %g is below the required %g (silent?)", peak, o.minPeak));

    if (const auto* params = session.extension<clap_plugin_params_t>(CLAP_EXT_PARAMS)) {
        in.clear();
        out.clear();
        params->flush(p, &ie, &oe); // must not crash with nothing pending
    }

    if (o.extra)
        o.extra(session, r);

    if (const auto* state = session.extension<clap_plugin_state_t>(CLAP_EXT_STATE)) {
        std::vector<std::uint8_t> a, b;
        r.check(saveState(p, state, a), "state save failed");
        r.check(loadState(p, state, a), "state load of the plugin's own state failed");
        if (o.stateRoundTrip) {
            r.check(saveState(p, state, b), "second state save failed");
            r.check(a == b, fmt("state is not stable over save, load, save (%zu vs %zu bytes)", a.size(), b.size()));
        }
    } else {
        r.notes.push_back("no state extension");
    }

#ifdef SPUX_HAS_X11
    if (o.gui)
        checkGui(session, host, r);
#else
    r.notes.push_back("built without X11; GUI part skipped");
#endif

    p->stop_processing(p);
    p->deactivate(p);
    p->destroy(p);
    return r;
}

} // namespace spux::inline v1::testing
