// What the editor needs from the plugin around it (guidelines 7, "Editor host"; from Plumeflow
// `ui/EditorHost`). The core is parameters (spux_params: table, host values, version, edit queue),
// the flush request, telemetry switching and the UI state; everything else is an optional extension
// a plugin offers only when it has it (modulation display, preset access). Widgets never see CLAP.
//
// Every method may be called from the editor's thread (the X11 event thread or the host's UI thread)
// while the host's main thread and the audio thread run; the implementation hands over as it needs.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "spux/params/EditQueue.hpp"
#include "spux/params/ParamStore.hpp"
#include "spux/params/ParamTable.hpp"
#include "spux/params/ParamText.hpp"

namespace spux::inline v1::ui {

// What the editor remembers per plugin instance (saved with the plugin's state if the plugin wants).
struct UiState {
    std::uint32_t page = 0;
    float scale = 1.0f;
};

// Optional: the live modulation of parameters and the armed-source depth handle (Plumeflow's matrix).
class ModulationHost {
public:
    struct Display {
        bool any = false;                // a route with a non-zero amount reaches the parameter
        double base = 0.0, shared = 0.0; // normalized: the value, and after the shared sources
        double voices[4] = {};
        int voiceCount = 0;
        bool armedRoute = false; // the armed source reaches the parameter
        double armedDepth = 0.0; // its extent at full source value (signed, normalized)
    };
    virtual ~ModulationHost() = default;
    // A source is armed: modulatable knobs show a depth handle.
    [[nodiscard]] virtual bool armed() const = 0;
    [[nodiscard]] virtual Display display(std::uint32_t paramId) = 0;
    // The parameter that holds the armed source's depth on `target`, the route made when there is none;
    // kNone when it cannot be made (every route in use). Dragging the handle edits that parameter.
    static constexpr std::uint32_t kNone = 0xFFFFFFFFu;
    virtual std::uint32_t depthParam(std::uint32_t target) = 0;
    // Pixels of vertical drag for the depth's whole range.
    [[nodiscard]] virtual float depthDragPixels() const { return 120.0f; }
};

// Optional: presets, for a preset browser (U10) or the plugin's own page code.
class PresetHost {
public:
    struct Entry {
        std::string name, category, path; // path: empty for a factory preset
        std::size_t index = 0;            // factory index
        bool factory = true;
    };
    enum class SaveStatus { Ok, Exists, Failed };
    virtual ~PresetHost() = default;
    [[nodiscard]] virtual std::string currentName() = 0;
    [[nodiscard]] virtual bool modified() { return false; }
    [[nodiscard]] virtual std::vector<Entry> factory() = 0;
    [[nodiscard]] virtual std::vector<Entry> user() { return {}; }
    virtual bool load(const Entry& e) = 0;
    virtual SaveStatus save(const std::string& /*name*/, const std::string& /*folder*/, bool /*overwrite*/) {
        return SaveStatus::Failed;
    }
    virtual SaveStatus rename(const Entry&, const std::string& /*newName*/) { return SaveStatus::Failed; }
    virtual bool remove(const Entry&) { return false; }
};

class EditorHost {
public:
    virtual ~EditorHost() = default;

    // --- parameters (spux_params) ---
    [[nodiscard]] virtual const params::ParamTable& paramTable() const = 0;
    [[nodiscard]] virtual double paramHost(std::size_t index) const = 0;
    // The editor's write of a host value: the sound follows at once; pushEdit tells the host.
    virtual void setParamHost(std::size_t index, double host) = 0;
    // Changes with every parameter write from anywhere (change-driven frames).
    [[nodiscard]] virtual std::uint32_t paramVersion() const = 0;
    // Queues a gesture begin, value or end for the host, keeping `reserve` slots free; false when full.
    virtual bool pushEdit(const params::ParamEdit& e, std::size_t reserve) = 0;
    // Slots a value leaves free for gesture marks (EditQueue::kGestureReserve).
    [[nodiscard]] virtual std::size_t gestureReserve() const { return params::EditQueue::kGestureReserve; }
    // Asks the host to run the parameter flush so queued edits reach it while no audio runs.
    virtual void requestFlush() = 0;

    // Value text and parsing; the defaults are spux's. A plugin with its own units overrides them.
    virtual void formatParam(std::size_t index, double plain, char* buf, std::size_t size) {
        params::formatValue(paramTable()[index], plain, buf, size);
    }
    virtual bool parseParam(std::size_t index, const char* text, double& plain) {
        return params::parseValue(paramTable()[index], text, plain);
    }
    // An extra tooltip line for a parameter (what it does), or null.
    [[nodiscard]] virtual const char* paramHint(std::size_t /*index*/) { return nullptr; }

    // --- telemetry: the audio side writes only while an editor shows it ---
    virtual void setTelemetryEnabled(bool /*on*/) {}

    // --- UI state ---
    [[nodiscard]] virtual UiState uiState() { return {}; }
    virtual void setUiState(const UiState&) {}

    // --- optional extensions ---
    [[nodiscard]] virtual ModulationHost* modulation() { return nullptr; }
    [[nodiscard]] virtual PresetHost* presets() { return nullptr; }
};

// The parameter part of EditorHost over a ParamStore and an edit queue, for a plugin built on
// spux_params. Derive and add requestFlush (and whatever else the plugin offers).
template <std::size_t N, class Queue = params::EditQueue> class StoreEditorHost : public EditorHost {
public:
    StoreEditorHost(params::ParamStore<N>& store, Queue& queue) : store_(store), queue_(queue) {}

    [[nodiscard]] const params::ParamTable& paramTable() const override { return store_.table(); }
    [[nodiscard]] double paramHost(std::size_t index) const override { return store_.host(index); }
    void setParamHost(std::size_t index, double host) override { store_.setHost(index, host); }
    [[nodiscard]] std::uint32_t paramVersion() const override { return store_.version(); }
    bool pushEdit(const params::ParamEdit& e, std::size_t reserve) override { return queue_.tryPush(e, reserve); }
    [[nodiscard]] std::size_t gestureReserve() const override { return Queue::kGestureReserve; }

protected:
    params::ParamStore<N>& store_;
    Queue& queue_;
};

} // namespace spux::inline v1::ui
