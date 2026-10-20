// Value text and parsing (plain values), from Plumeflow `plugin/Params`. A plugin with its own units
// formats its parameters itself and falls back to these for the rest (the CLAP glue calls the plugin).
//
//   Enum   the label                         parse: the exact label, or a number
//   Bool   "on" / "off"                      parse: "on", "off", or a number
//   Int    the rounded value                 parse: a number
//   Db     "%.1f dB", "-inf dB" at a minimum of -90 dB or below
//   Float  "%.4g"
// Parsed numbers are clamped to the range; the result is not sanitized further.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "spux/params/ParamInfo.hpp"

namespace spux::inline v1::params {

inline void formatValue(const ParamInfo& p, double v, char* buf, std::size_t size) {
    if (buf == nullptr || size == 0)
        return;
    if (p.kind == ParamKind::Enum) {
        const auto label = enumLabel(p, static_cast<std::size_t>(std::max(0L, std::lround(v - p.min))));
        std::snprintf(buf, size, "%.*s", static_cast<int>(label.size()), label.data());
    } else if (p.kind == ParamKind::Bool) {
        std::snprintf(buf, size, "%s", v >= 0.5 ? "on" : "off");
    } else if (p.kind == ParamKind::Int) {
        std::snprintf(buf, size, "%ld", std::lround(v));
    } else if (p.law == ParamLaw::Db) {
        if (v <= p.min && p.min <= -90.0)
            std::snprintf(buf, size, "-inf dB");
        else
            std::snprintf(buf, size, "%.1f dB", v);
    } else {
        std::snprintf(buf, size, "%.4g", v);
    }
}

inline bool parseValue(const ParamInfo& p, const char* text, double& out) {
    if (text == nullptr)
        return false;
    if (p.kind == ParamKind::Enum) {
        for (std::size_t i = 0, n = enumCount(p); i < n; ++i) {
            const auto l = enumLabel(p, i);
            if (std::strlen(text) == l.size() && std::strncmp(text, l.data(), l.size()) == 0) {
                out = p.min + static_cast<double>(i);
                return true;
            }
        }
    }
    if (p.kind == ParamKind::Bool) {
        if (std::strcmp(text, "on") == 0) {
            out = 1;
            return true;
        }
        if (std::strcmp(text, "off") == 0) {
            out = 0;
            return true;
        }
    }
    if (p.law == ParamLaw::Db && std::strncmp(text, "-inf", 4) == 0) {
        out = p.min;
        return true;
    }
    char* end = nullptr;
    const double v = std::strtod(text, &end);
    if (end == text || !std::isfinite(v))
        return false;
    out = std::clamp(v, p.min, p.max);
    return true;
}

} // namespace spux::inline v1::params
