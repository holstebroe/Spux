// Thin CLAP glue (guidelines section 6): a CRTP base that fills `clap_plugin_t`, forwards
// every call to the plugin class, and offers the extensions the plugin class implements. Not a
// framework: it owns no parameters, state or threads, and the plugin can implement any extension itself.
//
//   class MyPlugin : public spux::clap::Plugin<MyPlugin> {
//   public:
//       MyPlugin(const clap_host_t* host) : Plugin(&kDescriptor, host) {}
//       bool activate(double fs, uint32_t minFrames, uint32_t maxFrames);   // hides the default
//       clap_process_status process(const clap_process_t* p);
//       uint32_t paramsCount();  ...                                       // offers CLAP_EXT_PARAMS
//   };
//   const clap_plugin_factory_t& f = spux::clap::SingleFactory<MyPlugin>::factory;
//
// Lifecycle calls (init, destroy, activate, ...) have defaults the plugin hides by declaring its own.
// An extension is offered when the plugin class has its methods (checked with concepts, below):
//
//   params        paramsCount, paramsInfo, paramsValue, paramsValueToText, paramsTextToValue, paramsFlush
//   state         stateSave, stateLoad
//   audio ports   audioPortsCount(bool isInput), audioPortsGet(index, isInput, info)
//   note ports    notePortsCount(bool isInput), notePortsGet(index, isInput, info)
//   gui           guiCreate, guiDestroy, guiSetParent, guiGetSize, guiShow, guiHide; optional:
//                 guiIsApiSupported, guiGetPreferredApi, guiSetScale, guiCanResize, guiGetResizeHints,
//                 guiAdjustSize, guiSetSize, guiSetTransient, guiSuggestTitle (defaults: the platform's
//                 embedded window API, fixed size)
//   timer         onTimer(clap_id)
//
// The plugin's own getExtension(id) hides the default; it answers what it wants itself (a preset-load
// extension, its own params table) and calls defaultExtension(id) for the rest. Every call from the
// host is wrapped so that no C++ exception crosses into the host (the call then returns its failure
// value); the audio thread must not throw in the first place (R1).
#pragma once

#include <clap/clap.h>

#include <concepts>
#include <cstdint>
#include <cstring>

#include "spux/Version.hpp"

namespace spux::inline v1::clap {

template <class R, class F> R guarded(R fallback, F&& f) noexcept {
    try {
        return f();
    } catch (...) {
        return fallback;
    }
}
template <class F> void guardedVoid(F&& f) noexcept {
    try {
        f();
    } catch (...) {
    }
}

// The embedded-window API of this platform for the gui extension; kHasPlatformWindowApi is false where
// spux_platform has no window yet, and platformWindowApi() is then "".
#if defined(_WIN32)
inline constexpr bool kHasPlatformWindowApi = true;
constexpr const char* platformWindowApi() { return CLAP_WINDOW_API_WIN32; }
#elif defined(__linux__) || defined(__FreeBSD__)
inline constexpr bool kHasPlatformWindowApi = true;
constexpr const char* platformWindowApi() { return CLAP_WINDOW_API_X11; }
#else
inline constexpr bool kHasPlatformWindowApi = false; // macOS: Cocoa is not in spux_platform yet
constexpr const char* platformWindowApi() { return ""; }
#endif

// ---- What the plugin class provides ------------------------------------------------------------------

template <class D>
concept HasParams =
    requires(D& d, std::uint32_t i, clap_param_info_t* info, clap_id id, double* out, double v, char* buf,
             const char* text, const clap_input_events_t* in, const clap_output_events_t* outEv) {
        { d.paramsCount() } -> std::convertible_to<std::uint32_t>;
        { d.paramsInfo(i, info) } -> std::convertible_to<bool>;
        { d.paramsValue(id, out) } -> std::convertible_to<bool>;
        { d.paramsValueToText(id, v, buf, i) } -> std::convertible_to<bool>;
        { d.paramsTextToValue(id, text, out) } -> std::convertible_to<bool>;
        d.paramsFlush(in, outEv);
    };

template <class D>
concept HasState = requires(D& d, const clap_ostream_t* os, const clap_istream_t* is) {
    { d.stateSave(os) } -> std::convertible_to<bool>;
    { d.stateLoad(is) } -> std::convertible_to<bool>;
};

template <class D>
concept HasAudioPorts = requires(D& d, std::uint32_t i, bool input, clap_audio_port_info_t* info) {
    { d.audioPortsCount(input) } -> std::convertible_to<std::uint32_t>;
    { d.audioPortsGet(i, input, info) } -> std::convertible_to<bool>;
};

template <class D>
concept HasNotePorts = requires(D& d, std::uint32_t i, bool input, clap_note_port_info_t* info) {
    { d.notePortsCount(input) } -> std::convertible_to<std::uint32_t>;
    { d.notePortsGet(i, input, info) } -> std::convertible_to<bool>;
};

template <class D>
concept HasGui = requires(D& d, const char* api, bool floating, const clap_window_t* w, std::uint32_t* size) {
    { d.guiCreate(api, floating) } -> std::convertible_to<bool>;
    d.guiDestroy();
    { d.guiSetParent(w) } -> std::convertible_to<bool>;
    { d.guiGetSize(size, size) } -> std::convertible_to<bool>;
    { d.guiShow() } -> std::convertible_to<bool>;
    { d.guiHide() } -> std::convertible_to<bool>;
};

template <class D>
concept HasTimer = requires(D& d, clap_id id) { d.onTimer(id); };

template <class Derived> class Plugin;

namespace detail {

template <class D> D* self(const clap_plugin_t* p) { return static_cast<D*>(static_cast<Plugin<D>*>(p->plugin_data)); }

template <class D> struct ParamsExt {
    static constexpr clap_plugin_params_t table = {
        [](const clap_plugin_t* p) -> std::uint32_t {
            return guarded<std::uint32_t>(0, [&] { return static_cast<std::uint32_t>(self<D>(p)->paramsCount()); });
        },
        [](const clap_plugin_t* p, std::uint32_t i, clap_param_info_t* info) -> bool {
            return info != nullptr &&
                   guarded(false, [&] { return static_cast<bool>(self<D>(p)->paramsInfo(i, info)); });
        },
        [](const clap_plugin_t* p, clap_id id, double* v) -> bool {
            return v != nullptr && guarded(false, [&] { return static_cast<bool>(self<D>(p)->paramsValue(id, v)); });
        },
        [](const clap_plugin_t* p, clap_id id, double v, char* buf, std::uint32_t cap) -> bool {
            return buf != nullptr && cap > 0 &&
                   guarded(false, [&] { return static_cast<bool>(self<D>(p)->paramsValueToText(id, v, buf, cap)); });
        },
        [](const clap_plugin_t* p, clap_id id, const char* text, double* v) -> bool {
            return text != nullptr && v != nullptr &&
                   guarded(false, [&] { return static_cast<bool>(self<D>(p)->paramsTextToValue(id, text, v)); });
        },
        [](const clap_plugin_t* p, const clap_input_events_t* in, const clap_output_events_t* out) {
            guardedVoid([&] { self<D>(p)->paramsFlush(in, out); });
        },
    };
};

template <class D> struct StateExt {
    static constexpr clap_plugin_state_t table = {
        [](const clap_plugin_t* p, const clap_ostream_t* s) -> bool {
            return s != nullptr && guarded(false, [&] { return static_cast<bool>(self<D>(p)->stateSave(s)); });
        },
        [](const clap_plugin_t* p, const clap_istream_t* s) -> bool {
            return s != nullptr && guarded(false, [&] { return static_cast<bool>(self<D>(p)->stateLoad(s)); });
        },
    };
};

template <class D> struct AudioPortsExt {
    static constexpr clap_plugin_audio_ports_t table = {
        [](const clap_plugin_t* p, bool input) -> std::uint32_t {
            return guarded<std::uint32_t>(
                0, [&] { return static_cast<std::uint32_t>(self<D>(p)->audioPortsCount(input)); });
        },
        [](const clap_plugin_t* p, std::uint32_t i, bool input, clap_audio_port_info_t* info) -> bool {
            return info != nullptr &&
                   guarded(false, [&] { return static_cast<bool>(self<D>(p)->audioPortsGet(i, input, info)); });
        },
    };
};

template <class D> struct NotePortsExt {
    static constexpr clap_plugin_note_ports_t table = {
        [](const clap_plugin_t* p, bool input) -> std::uint32_t {
            return guarded<std::uint32_t>(
                0, [&] { return static_cast<std::uint32_t>(self<D>(p)->notePortsCount(input)); });
        },
        [](const clap_plugin_t* p, std::uint32_t i, bool input, clap_note_port_info_t* info) -> bool {
            return info != nullptr &&
                   guarded(false, [&] { return static_cast<bool>(self<D>(p)->notePortsGet(i, input, info)); });
        },
    };
};

template <class D> struct GuiExt {
    static bool sizeOf(D& d, std::uint32_t& w, std::uint32_t& h) { return static_cast<bool>(d.guiGetSize(&w, &h)); }

    static constexpr clap_plugin_gui_t table = {
        [](const clap_plugin_t* p, const char* api, bool floating) -> bool {
            return guarded(false, [&] {
                [[maybe_unused]] D& d = *self<D>(p);
                if constexpr (requires { d.guiIsApiSupported(api, floating); })
                    return static_cast<bool>(d.guiIsApiSupported(api, floating));
                else
                    return kHasPlatformWindowApi && api != nullptr && !floating &&
                           std::strcmp(api, platformWindowApi()) == 0;
            });
        },
        [](const clap_plugin_t* p, const char** api, bool* floating) -> bool {
            if (api == nullptr || floating == nullptr)
                return false;
            return guarded(false, [&] {
                [[maybe_unused]] D& d = *self<D>(p);
                if constexpr (requires { d.guiGetPreferredApi(api, floating); })
                    return static_cast<bool>(d.guiGetPreferredApi(api, floating));
                else {
                    if constexpr (!kHasPlatformWindowApi)
                        return false;
                    *api = platformWindowApi();
                    *floating = false;
                    return true;
                }
            });
        },
        [](const clap_plugin_t* p, const char* api, bool floating) -> bool {
            return guarded(false, [&] { return static_cast<bool>(self<D>(p)->guiCreate(api, floating)); });
        },
        [](const clap_plugin_t* p) { guardedVoid([&] { self<D>(p)->guiDestroy(); }); },
        [](const clap_plugin_t* p, [[maybe_unused]] double scale) -> bool {
            return guarded(false, [&] {
                [[maybe_unused]] D& d = *self<D>(p);
                if constexpr (requires { d.guiSetScale(scale); })
                    return static_cast<bool>(d.guiSetScale(scale));
                else
                    return false; // logical pixels; the host scales
            });
        },
        [](const clap_plugin_t* p, std::uint32_t* w, std::uint32_t* h) -> bool {
            return w != nullptr && h != nullptr &&
                   guarded(false, [&] { return static_cast<bool>(self<D>(p)->guiGetSize(w, h)); });
        },
        [](const clap_plugin_t* p) -> bool {
            return guarded(false, [&] {
                [[maybe_unused]] D& d = *self<D>(p);
                if constexpr (requires { d.guiCanResize(); })
                    return static_cast<bool>(d.guiCanResize());
                else
                    return false;
            });
        },
        [](const clap_plugin_t* p, clap_gui_resize_hints_t* hints) -> bool {
            return hints != nullptr && guarded(false, [&] {
                       [[maybe_unused]] D& d = *self<D>(p);
                       if constexpr (requires { d.guiGetResizeHints(hints); })
                           return static_cast<bool>(d.guiGetResizeHints(hints));
                       else
                           return false;
                   });
        },
        [](const clap_plugin_t* p, std::uint32_t* w, std::uint32_t* h) -> bool {
            if (w == nullptr || h == nullptr)
                return false;
            return guarded(false, [&] {
                [[maybe_unused]] D& d = *self<D>(p);
                if constexpr (requires { d.guiAdjustSize(w, h); })
                    return static_cast<bool>(d.guiAdjustSize(w, h));
                else
                    return sizeOf(d, *w, *h); // fixed size: the only size there is
            });
        },
        [](const clap_plugin_t* p, std::uint32_t w, std::uint32_t h) -> bool {
            return guarded(false, [&] {
                [[maybe_unused]] D& d = *self<D>(p);
                if constexpr (requires { d.guiSetSize(w, h); })
                    return static_cast<bool>(d.guiSetSize(w, h));
                else {
                    std::uint32_t cw = 0, ch = 0;
                    return sizeOf(d, cw, ch) && cw == w && ch == h;
                }
            });
        },
        [](const clap_plugin_t* p, const clap_window_t* window) -> bool {
            return window != nullptr &&
                   guarded(false, [&] { return static_cast<bool>(self<D>(p)->guiSetParent(window)); });
        },
        [](const clap_plugin_t* p, [[maybe_unused]] const clap_window_t* window) -> bool {
            return guarded(false, [&] {
                [[maybe_unused]] D& d = *self<D>(p);
                if constexpr (requires { d.guiSetTransient(window); })
                    return static_cast<bool>(d.guiSetTransient(window));
                else
                    return false; // embedded windows only
            });
        },
        [](const clap_plugin_t* p, [[maybe_unused]] const char* title) {
            guardedVoid([&] {
                [[maybe_unused]] D& d = *self<D>(p);
                if constexpr (requires { d.guiSuggestTitle(title); })
                    d.guiSuggestTitle(title);
            });
        },
        [](const clap_plugin_t* p) -> bool {
            return guarded(false, [&] { return static_cast<bool>(self<D>(p)->guiShow()); });
        },
        [](const clap_plugin_t* p) -> bool {
            return guarded(false, [&] { return static_cast<bool>(self<D>(p)->guiHide()); });
        },
    };
};

template <class D> struct TimerExt {
    static constexpr clap_plugin_timer_support_t table = {
        [](const clap_plugin_t* p, clap_id id) { guardedVoid([&] { self<D>(p)->onTimer(id); }); },
    };
};

} // namespace detail

template <class Derived> class Plugin {
public:
    Plugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host) : host_(host) {
        plugin_.desc = descriptor;
        plugin_.plugin_data = this;
        plugin_.init = [](const clap_plugin_t* p) -> bool {
            return guarded(false, [&] { return static_cast<bool>(detail::self<Derived>(p)->init()); });
        };
        plugin_.destroy = [](const clap_plugin_t* p) { guardedVoid([&] { detail::self<Derived>(p)->destroy(); }); };
        plugin_.activate = [](const clap_plugin_t* p, double sr, std::uint32_t mn, std::uint32_t mx) -> bool {
            return guarded(false, [&] { return static_cast<bool>(detail::self<Derived>(p)->activate(sr, mn, mx)); });
        };
        plugin_.deactivate = [](const clap_plugin_t* p) {
            guardedVoid([&] { detail::self<Derived>(p)->deactivate(); });
        };
        plugin_.start_processing = [](const clap_plugin_t* p) -> bool {
            return guarded(false, [&] { return static_cast<bool>(detail::self<Derived>(p)->startProcessing()); });
        };
        plugin_.stop_processing = [](const clap_plugin_t* p) {
            guardedVoid([&] { detail::self<Derived>(p)->stopProcessing(); });
        };
        plugin_.reset = [](const clap_plugin_t* p) { guardedVoid([&] { detail::self<Derived>(p)->reset(); }); };
        plugin_.process = [](const clap_plugin_t* p, const clap_process_t* pr) -> clap_process_status {
            if (pr == nullptr)
                return CLAP_PROCESS_ERROR;
            return guarded<clap_process_status>(CLAP_PROCESS_ERROR,
                                                [&] { return detail::self<Derived>(p)->process(pr); });
        };
        plugin_.get_extension = [](const clap_plugin_t* p, const char* id) -> const void* {
            if (id == nullptr)
                return nullptr;
            return guarded<const void*>(nullptr, [&] { return detail::self<Derived>(p)->getExtension(id); });
        };
        plugin_.on_main_thread = [](const clap_plugin_t* p) {
            guardedVoid([&] { detail::self<Derived>(p)->onMainThread(); });
        };
    }
    Plugin(const Plugin&) = delete;
    Plugin& operator=(const Plugin&) = delete;

    [[nodiscard]] const clap_plugin_t* clapPlugin() const { return &plugin_; }
    [[nodiscard]] const clap_host_t* host() const { return host_; }

    // ---- Defaults (hide them by declaring the same function in the plugin class) ----
    bool init() { return true; }
    void destroy() { delete static_cast<Derived*>(this); }
    bool activate(double, std::uint32_t, std::uint32_t) { return true; }
    void deactivate() {}
    bool startProcessing() { return true; }
    void stopProcessing() {}
    void reset() {}
    clap_process_status process(const clap_process_t*) { return CLAP_PROCESS_CONTINUE; }
    void onMainThread() {}
    const void* getExtension(const char* id) { return defaultExtension(id); }

    // The extensions the plugin class implements (see the top of this file).
    const void* defaultExtension(const char* id) {
        if (id == nullptr)
            return nullptr;
        if constexpr (HasParams<Derived>)
            if (std::strcmp(id, CLAP_EXT_PARAMS) == 0)
                return &detail::ParamsExt<Derived>::table;
        if constexpr (HasState<Derived>)
            if (std::strcmp(id, CLAP_EXT_STATE) == 0)
                return &detail::StateExt<Derived>::table;
        if constexpr (HasAudioPorts<Derived>)
            if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0)
                return &detail::AudioPortsExt<Derived>::table;
        if constexpr (HasNotePorts<Derived>)
            if (std::strcmp(id, CLAP_EXT_NOTE_PORTS) == 0)
                return &detail::NotePortsExt<Derived>::table;
        if constexpr (HasGui<Derived>)
            if (std::strcmp(id, CLAP_EXT_GUI) == 0)
                return &detail::GuiExt<Derived>::table;
        if constexpr (HasTimer<Derived>)
            if (std::strcmp(id, CLAP_EXT_TIMER_SUPPORT) == 0)
                return &detail::TimerExt<Derived>::table;
        return nullptr;
    }

    // ---- Calls to the host (null-safe; the host may lack any extension) ----
    template <class T> [[nodiscard]] const T* hostExtension(const char* id) const {
        if (host_ == nullptr || host_->get_extension == nullptr)
            return nullptr;
        return static_cast<const T*>(host_->get_extension(host_, id));
    }
    void requestCallback() const {
        if (host_ != nullptr && host_->request_callback != nullptr)
            host_->request_callback(host_);
    }
    void requestProcess() const {
        if (host_ != nullptr && host_->request_process != nullptr)
            host_->request_process(host_);
    }
    void requestRestart() const {
        if (host_ != nullptr && host_->request_restart != nullptr)
            host_->request_restart(host_);
    }
    // Asks for a params flush (or a process call), so queued editor edits reach the host.
    void requestParamsFlush() const {
        const auto* hp = hostExtension<clap_host_params_t>(CLAP_EXT_PARAMS);
        if (hp != nullptr && hp->request_flush != nullptr)
            hp->request_flush(host_);
        else
            requestProcess();
    }
    // Main thread.
    void rescanParams(clap_param_rescan_flags flags) const {
        const auto* hp = hostExtension<clap_host_params_t>(CLAP_EXT_PARAMS);
        if (hp != nullptr && hp->rescan != nullptr)
            hp->rescan(host_, flags);
    }
    // Main thread.
    void markStateDirty() const {
        const auto* hs = hostExtension<clap_host_state_t>(CLAP_EXT_STATE);
        if (hs != nullptr && hs->mark_dirty != nullptr)
            hs->mark_dirty(host_);
    }
    // Main thread. False when the host has no timer support.
    bool registerTimer(std::uint32_t periodMs, clap_id* id) const {
        const auto* ht = hostExtension<clap_host_timer_support_t>(CLAP_EXT_TIMER_SUPPORT);
        return ht != nullptr && ht->register_timer != nullptr && id != nullptr &&
               ht->register_timer(host_, periodMs, id);
    }
    bool unregisterTimer(clap_id id) const {
        const auto* ht = hostExtension<clap_host_timer_support_t>(CLAP_EXT_TIMER_SUPPORT);
        return ht != nullptr && ht->unregister_timer != nullptr && ht->unregister_timer(host_, id);
    }

protected:
    ~Plugin() = default;

private:
    clap_plugin_t plugin_{};
    const clap_host_t* host_ = nullptr;
};

// A plugin factory with one plugin class. P provides `static const clap_plugin_descriptor_t& descriptor()`
// and a constructor P(const clap_host_t*). Plugins with several classes write their own factory.
template <class P> struct SingleFactory {
    static constexpr clap_plugin_factory_t factory = {
        [](const clap_plugin_factory_t*) -> std::uint32_t { return 1; },
        [](const clap_plugin_factory_t*, std::uint32_t i) -> const clap_plugin_descriptor_t* {
            return i == 0 ? &P::descriptor() : nullptr;
        },
        [](const clap_plugin_factory_t*, const clap_host_t* host, const char* id) -> const clap_plugin_t* {
            if (host == nullptr || id == nullptr || !clap_version_is_compatible(host->clap_version))
                return nullptr;
            if (std::strcmp(id, P::descriptor().id) != 0)
                return nullptr;
            return guarded<const clap_plugin_t*>(nullptr, [&] { return (new P(host))->clapPlugin(); });
        },
    };
};

} // namespace spux::inline v1::clap
