// A small CLAP instrument used to test the smoke host: two parameters, a state blob, a stereo output, a
// note input and (with X11) a GUI that embeds a child window. SPUX_TEST_FAULT builds a broken variant:
//   1 NaN in the output   2 output above full scale   3 state that changes on every save
//   4 parameter events ignored   5 a default value outside the parameter's range
#include <clap/clap.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef SPUX_HAS_X11
#include <X11/Xlib.h>
#endif

#ifndef SPUX_TEST_FAULT
#define SPUX_TEST_FAULT 0
#endif

namespace {

struct Plugin {
    clap_plugin_t plugin{};
    const clap_host_t* host = nullptr;
    double gain = 0.5, tune = 0.0, rate = 48000.0, phase = 0.0;
    int held = 0;
    int saves = 0;
#ifdef SPUX_HAS_X11
    Display* dpy = nullptr;
    Window child = 0;
    clap_id timer = 0;
#endif
};
Plugin* self(const clap_plugin_t* p) { return static_cast<Plugin*>(p->plugin_data); }

// --- params ---
uint32_t pCount(const clap_plugin_t*) { return 2; }
bool pInfo(const clap_plugin_t*, uint32_t i, clap_param_info_t* info) {
    if (i > 1)
        return false;
    std::memset(info, 0, sizeof(*info));
    info->id = i;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    std::strcpy(info->name, i == 0 ? "Gain" : "Tune");
    info->min_value = i == 0 ? 0.0 : -12.0;
    info->max_value = i == 0 ? 1.0 : 12.0;
    info->default_value = i == 0 ? 0.5 : 0.0;
    if (SPUX_TEST_FAULT == 5 && i == 1)
        info->default_value = 20.0;
    return true;
}
bool pGet(const clap_plugin_t* p, clap_id id, double* v) {
    if (id > 1)
        return false;
    *v = id == 0 ? self(p)->gain : self(p)->tune;
    return true;
}
bool pToText(const clap_plugin_t*, clap_id id, double v, char* out, uint32_t n) {
    std::snprintf(out, n, id == 0 ? "%.3f" : "%.2f st", v);
    return true;
}
bool pFromText(const clap_plugin_t*, clap_id, const char* text, double* v) {
    *v = std::atof(text);
    return true;
}
void applyParam(Plugin* s, const clap_event_param_value_t* e) {
    if (SPUX_TEST_FAULT == 4)
        return;
    (e->param_id == 0 ? s->gain : s->tune) = e->value;
}
void pFlush(const clap_plugin_t* p, const clap_input_events_t* in, const clap_output_events_t*) {
    for (uint32_t i = 0; i < in->size(in); ++i) {
        const clap_event_header_t* h = in->get(in, i);
        if (h->space_id == CLAP_CORE_EVENT_SPACE_ID && h->type == CLAP_EVENT_PARAM_VALUE)
            applyParam(self(p), reinterpret_cast<const clap_event_param_value_t*>(h));
    }
}
const clap_plugin_params_t kParams = {pCount, pInfo, pGet, pToText, pFromText, pFlush};

// --- state ---
bool sSave(const clap_plugin_t* p, const clap_ostream_t* os) {
    Plugin* s = self(p);
    double blob[3] = {s->gain, s->tune, SPUX_TEST_FAULT == 3 ? static_cast<double>(++s->saves) : 0.0};
    return os->write(os, blob, sizeof(blob)) == static_cast<int64_t>(sizeof(blob));
}
bool sLoad(const clap_plugin_t* p, const clap_istream_t* is) {
    double blob[3];
    if (is->read(is, blob, sizeof(blob)) != static_cast<int64_t>(sizeof(blob)))
        return false;
    self(p)->gain = blob[0];
    self(p)->tune = blob[1];
    return true;
}
const clap_plugin_state_t kState = {sSave, sLoad};

// --- ports ---
uint32_t apCount(const clap_plugin_t*, bool isInput) { return isInput ? 0 : 1; }
bool apGet(const clap_plugin_t*, uint32_t i, bool isInput, clap_audio_port_info_t* info) {
    if (i != 0 || isInput)
        return false;
    std::memset(info, 0, sizeof(*info));
    info->id = 0;
    std::strcpy(info->name, "out");
    info->channel_count = 2;
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->port_type = CLAP_PORT_STEREO;
    return true;
}
const clap_plugin_audio_ports_t kAudioPorts = {apCount, apGet};
uint32_t npCount(const clap_plugin_t*, bool isInput) { return isInput ? 1 : 0; }
bool npGet(const clap_plugin_t*, uint32_t i, bool isInput, clap_note_port_info_t* info) {
    if (i != 0 || !isInput)
        return false;
    info->id = 0;
    info->supported_dialects = CLAP_NOTE_DIALECT_CLAP;
    info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
    std::strcpy(info->name, "notes");
    return true;
}
const clap_plugin_note_ports_t kNotePorts = {npCount, npGet};

// --- gui (X11 only) ---
#ifdef SPUX_HAS_X11
bool gSupported(const clap_plugin_t*, const char* api, bool floating) {
    return !floating && !std::strcmp(api, CLAP_WINDOW_API_X11);
}
bool gCreate(const clap_plugin_t* p, const char*, bool) {
    self(p)->dpy = XOpenDisplay(nullptr);
    return self(p)->dpy != nullptr;
}
void gDestroy(const clap_plugin_t* p) {
    Plugin* s = self(p);
    if (s->dpy) {
        if (s->child)
            XDestroyWindow(s->dpy, s->child);
        XCloseDisplay(s->dpy);
    }
    s->dpy = nullptr;
    s->child = 0;
}
bool gSize(const clap_plugin_t*, uint32_t* w, uint32_t* h) {
    *w = 320;
    *h = 200;
    return true;
}
bool gParent(const clap_plugin_t* p, const clap_window_t* w) {
    Plugin* s = self(p);
    s->child = XCreateSimpleWindow(s->dpy, static_cast<Window>(w->x11), 0, 0, 320, 200, 0, 0, 0);
    return s->child != 0;
}
bool gShow(const clap_plugin_t* p) {
    Plugin* s = self(p);
    XMapWindow(s->dpy, s->child);
    XSync(s->dpy, False);
    const clap_host_timer_support_t* t =
        static_cast<const clap_host_timer_support_t*>(s->host->get_extension(s->host, CLAP_EXT_TIMER_SUPPORT));
    return !t || t->register_timer(s->host, 30, &s->timer);
}
bool gHide(const clap_plugin_t* p) {
    Plugin* s = self(p);
    const clap_host_timer_support_t* t =
        static_cast<const clap_host_timer_support_t*>(s->host->get_extension(s->host, CLAP_EXT_TIMER_SUPPORT));
    if (t && s->timer)
        t->unregister_timer(s->host, s->timer);
    s->timer = 0;
    XUnmapWindow(s->dpy, s->child);
    return true;
}
const clap_plugin_gui_t kGui = {gSupported,
                                [](const clap_plugin_t*, const char**, bool*) { return false; },
                                gCreate,
                                gDestroy,
                                [](const clap_plugin_t*, double) { return true; },
                                gSize,
                                [](const clap_plugin_t*) { return false; },
                                [](const clap_plugin_t*, clap_gui_resize_hints_t*) { return false; },
                                [](const clap_plugin_t*, uint32_t*, uint32_t*) { return false; },
                                [](const clap_plugin_t*, uint32_t, uint32_t) { return true; },
                                gParent,
                                [](const clap_plugin_t*, const clap_window_t*) { return false; },
                                [](const clap_plugin_t*, const char*) {},
                                gShow,
                                gHide};
const clap_plugin_timer_support_t kTimer = {[](const clap_plugin_t*, clap_id) {}};
#endif

// --- plugin ---
bool init(const clap_plugin_t*) { return true; }
void destroy(const clap_plugin_t* p) { delete self(p); }
bool activate(const clap_plugin_t* p, double rate, uint32_t, uint32_t) {
    self(p)->rate = rate;
    return true;
}
void deactivate(const clap_plugin_t*) {}
bool startProcessing(const clap_plugin_t*) { return true; }
void stopProcessing(const clap_plugin_t*) {}
void reset(const clap_plugin_t*) {}
clap_process_status process(const clap_plugin_t* p, const clap_process_t* pr) {
    Plugin* s = self(p);
    uint32_t next = 0;
    const uint32_t n = pr->in_events->size(pr->in_events);
    float* l = pr->audio_outputs[0].data32[0];
    float* r = pr->audio_outputs[0].data32[1];
    for (uint32_t i = 0; i < pr->frames_count; ++i) {
        while (next < n && pr->in_events->get(pr->in_events, next)->time <= i) {
            const clap_event_header_t* h = pr->in_events->get(pr->in_events, next++);
            if (h->space_id != CLAP_CORE_EVENT_SPACE_ID)
                continue;
            if (h->type == CLAP_EVENT_NOTE_ON)
                ++s->held;
            else if (h->type == CLAP_EVENT_NOTE_OFF && s->held > 0)
                --s->held;
            else if (h->type == CLAP_EVENT_PARAM_VALUE)
                applyParam(s, reinterpret_cast<const clap_event_param_value_t*>(h));
        }
        const double amp = s->held > 0 ? 0.2 * s->gain * (SPUX_TEST_FAULT == 2 ? 10.0 : 1.0) : 0.0;
        s->phase += 2.0 * 3.14159265358979 * 220.0 * std::pow(2.0, s->tune / 12.0) / s->rate;
        if (s->phase > 6.283185307179586)
            s->phase -= 6.283185307179586;
        float v = static_cast<float>(amp * std::sin(s->phase));
        if (SPUX_TEST_FAULT == 1 && i == 7 && s->held > 0)
            v = std::nanf("");
        l[i] = r[i] = v;
    }
    return CLAP_PROCESS_CONTINUE;
}
const void* getExtension(const clap_plugin_t*, const char* id) {
    if (!std::strcmp(id, CLAP_EXT_PARAMS))
        return &kParams;
    if (!std::strcmp(id, CLAP_EXT_STATE))
        return &kState;
    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS))
        return &kAudioPorts;
    if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS))
        return &kNotePorts;
#ifdef SPUX_HAS_X11
    if (!std::strcmp(id, CLAP_EXT_GUI))
        return &kGui;
    if (!std::strcmp(id, CLAP_EXT_TIMER_SUPPORT))
        return &kTimer;
#endif
    return nullptr;
}
void onMainThread(const clap_plugin_t*) {}

const char* kFeatures[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER, nullptr};
const clap_plugin_descriptor_t kDescriptor = {CLAP_VERSION_INIT,
                                              "org.spux.test-plugin",
                                              "Spux test plugin",
                                              "spux",
                                              "",
                                              "",
                                              "",
                                              "1.0",
                                              "A plugin for testing the smoke host",
                                              kFeatures};

uint32_t fCount(const clap_plugin_factory_t*) { return 1; }
const clap_plugin_descriptor_t* fDesc(const clap_plugin_factory_t*, uint32_t i) {
    return i == 0 ? &kDescriptor : nullptr;
}
const clap_plugin_t* fCreate(const clap_plugin_factory_t*, const clap_host_t* host, const char* id) {
    if (std::strcmp(id, kDescriptor.id) != 0)
        return nullptr;
    auto* s = new Plugin;
    s->host = host;
    s->plugin = {&kDescriptor,   s,     init,    destroy,      activate,    deactivate, startProcessing,
                 stopProcessing, reset, process, getExtension, onMainThread};
    return &s->plugin;
}
const clap_plugin_factory_t kFactory = {fCount, fDesc, fCreate};

bool entryInit(const char*) { return true; }
void entryDeinit() {}
const void* entryFactory(const char* id) { return !std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? &kFactory : nullptr; }

} // namespace

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {CLAP_VERSION_INIT, entryInit, entryDeinit, entryFactory};
