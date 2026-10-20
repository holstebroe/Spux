// Small CLAP helpers for plugins on spux_params and spux_state: parameter info from a ParamInfo, the
// default value text, parameter events in and out, the editor's EditQueue drained into host events,
// and whole-stream reads and writes for the state extension. Each is a free function the plugin calls
// where it wants it (the glue never calls them on its own).
#pragma once

#include <clap/clap.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <vector>

#include "spux/params/EditQueue.hpp"
#include "spux/params/ParamInfo.hpp"
#include "spux/params/ParamTable.hpp"
#include "spux/params/ParamText.hpp"

namespace spux::inline v1::clap {

// Fills `info` from a ParamInfo: id, name, module, host range and default; flags from PF_AUTO,
// PF_MODULATABLE, PF_STEPPED, and CLAP_PARAM_IS_ENUM for Enum parameters.
inline void fillParamInfo(const params::ParamInfo& p, clap_param_info_t& info) {
    std::memset(&info, 0, sizeof(info));
    info.id = p.id;
    if (p.flags & params::PF_AUTO)
        info.flags |= CLAP_PARAM_IS_AUTOMATABLE;
    if (p.flags & params::PF_MODULATABLE)
        info.flags |= CLAP_PARAM_IS_MODULATABLE;
    if (p.flags & params::PF_STEPPED)
        info.flags |= CLAP_PARAM_IS_STEPPED;
    if (p.kind == params::ParamKind::Enum)
        info.flags |= CLAP_PARAM_IS_ENUM;
    std::snprintf(info.name, sizeof(info.name), "%s", p.name);
    std::snprintf(info.module, sizeof(info.module), "%s", p.module);
    info.min_value = params::hostMin(p);
    info.max_value = params::hostMax(p);
    info.default_value = params::hostDefault(p);
}

// The default text of a host value, and back (ParamText.hpp).
inline bool hostValueToText(const params::ParamInfo& p, double host, char* buf, std::uint32_t cap) {
    if (buf == nullptr || cap == 0)
        return false;
    params::formatValue(p, params::hostToPlain(p, host), buf, cap);
    return true;
}
inline bool textToHostValue(const params::ParamInfo& p, const char* text, double* host) {
    double plain = 0.0;
    if (host == nullptr || !params::parseValue(p, text, plain))
        return false;
    *host = params::plainToHost(p, plain);
    return true;
}

// A core-space parameter value event, or nullptr for anything else (size checked).
inline const clap_event_param_value_t* asParamValue(const clap_event_header_t* h) {
    if (h == nullptr || h->space_id != CLAP_CORE_EVENT_SPACE_ID || h->type != CLAP_EVENT_PARAM_VALUE ||
        h->size < sizeof(clap_event_param_value_t))
        return nullptr;
    return reinterpret_cast<const clap_event_param_value_t*>(h);
}

inline bool pushParamValue(const clap_output_events_t* out, std::uint32_t time, clap_id id, double value,
                           std::uint32_t flags = CLAP_EVENT_IS_LIVE) {
    if (out == nullptr || out->try_push == nullptr)
        return false;
    clap_event_param_value_t ev{};
    ev.header = {sizeof(ev), time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, flags};
    ev.param_id = id;
    ev.cookie = nullptr;
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.value = value;
    return out->try_push(out, &ev.header);
}

inline bool pushGesture(const clap_output_events_t* out, std::uint32_t time, clap_id id, bool begin,
                        std::uint32_t flags = CLAP_EVENT_IS_LIVE) {
    if (out == nullptr || out->try_push == nullptr)
        return false;
    clap_event_param_gesture_t ev{};
    ev.header = {sizeof(ev), time, CLAP_CORE_EVENT_SPACE_ID,
                 static_cast<std::uint16_t>(begin ? CLAP_EVENT_PARAM_GESTURE_BEGIN : CLAP_EVENT_PARAM_GESTURE_END),
                 flags};
    ev.param_id = id;
    return out->try_push(out, &ev.header);
}

// Audio thread or flush: turns the editor's queued edits into gesture and value events at `time`
// (Plumeflow's drainEdits). Indices are table indices; edits for an unknown index are dropped. Without
// an output queue the edits are dropped too (the store has the values already).
template <std::size_t C, std::size_t R>
void drainEdits(params::BasicEditQueue<C, R>& queue, const params::ParamTable& table, const clap_output_events_t* out,
                std::uint32_t time = 0) {
    params::ParamEdit e;
    while (queue.pop(e)) {
        if (e.index >= table.size() || out == nullptr)
            continue;
        const clap_id id = table[e.index].id;
        if (e.kind == params::ParamEdit::Kind::Value)
            pushParamValue(out, time, id, e.host);
        else
            pushGesture(out, time, id, e.kind == params::ParamEdit::Kind::Begin);
    }
}

// Writes all bytes to a host stream (the host may take them in parts).
inline bool writeAll(const clap_ostream_t* stream, std::span<const std::uint8_t> bytes) {
    if (stream == nullptr || stream->write == nullptr)
        return false;
    std::size_t done = 0;
    while (done < bytes.size()) {
        const std::int64_t n = stream->write(stream, bytes.data() + done, bytes.size() - done);
        if (n <= 0)
            return false;
        done += static_cast<std::size_t>(n);
    }
    return true;
}

// Reads a host stream to its end into `out`. False on a read error or when it holds more than
// maxBytes (then it is not a state of ours).
inline bool readAll(const clap_istream_t* stream, std::vector<std::uint8_t>& out, std::size_t maxBytes = 16u << 20) {
    out.clear();
    if (stream == nullptr || stream->read == nullptr)
        return false;
    std::uint8_t chunk[4096];
    for (;;) {
        const std::int64_t n = stream->read(stream, chunk, sizeof(chunk));
        if (n < 0)
            return false;
        if (n == 0)
            return true;
        const std::size_t got = std::min<std::size_t>(static_cast<std::size_t>(n), sizeof(chunk));
        if (out.size() + got > maxBytes)
            return false;
        out.insert(out.end(), chunk, chunk + got);
    }
}

} // namespace spux::inline v1::clap
