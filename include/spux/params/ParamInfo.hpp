// Parameter description and laws (guidelines section 6).
//
// A plugin lists its parameters once, in an X-macro `.def` file (ParamTable.hpp has the macros); each
// row becomes a ParamInfo. Ids and keys are stable: never renumber an id or rename a key, only append.
//
// Values come in three domains:
//   plain       the displayed unit (ms, Hz, dB, an enum index), what min, max and def are given in;
//   normalized  0..1 through the law: the position of a control;
//   host        what the host stores and automates: normalized for Float parameters with a Log, LogZero
//               or Db law (perceptually even automation), the plain value otherwise. PF_HOST_PLAIN keeps
//               the plain value for any parameter (a plugin whose released automation is in dB).
//
// Laws count as sound: the host stores normalized automation, so a changed law changes old
// projects. A different law is a new ParamLaw value; the existing ones never change their curve.
//   Linear   n = (v - min) / (max - min)
//   Log      n = ln(v / min) / ln(max / min)                                   (min > 0)
//   LogZero  0 at the bottom, a linear knee to 0.01 (plain) over the first 1 % of the control, then
//            logarithmic from 0.01 to max (times that reach exactly 0)
//   Db       linear in dB, like Linear; the host sees it normalized
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "spux/Version.hpp"

namespace spux::inline v1::params {

enum class ParamKind : std::uint8_t { Float, Int, Bool, Enum };
enum class ParamLaw : std::uint8_t { Linear, Log, LogZero, Db };

// Flags column of the .def file.
enum ParamFlags : std::uint32_t {
    PF_AUTO = 1,        // automatable
    PF_MODULATABLE = 2, // a modulation destination (host modulation, CLAP_PARAM_IS_MODULATABLE)
    PF_STEPPED = 4,     // the host sees discrete steps
    PF_PER_VOICE = 8,   // polyphonic modulation per voice
    PF_HOST_PLAIN = 16, // the host value is the plain value, whatever the law
};

struct ParamInfo {
    std::uint32_t id = 0;
    const char* key = "";  // stable, used by state and presets
    const char* name = ""; // shown to the user; may change
    ParamKind kind = ParamKind::Float;
    double min = 0.0;
    double max = 1.0;
    ParamLaw law = ParamLaw::Linear;
    double def = 0.0;
    std::uint32_t flags = 0;
    const char* labels = ""; // Enum: labels separated by '|', index 0 at min
    const char* module = ""; // the host's grouping ("Filter", "Output"); may be empty
};

namespace detail {
inline constexpr double kLogZeroFloor = 0.01; // plain value where the log part starts
inline constexpr double kLogZeroKnee = 0.01;  // normalized position of the floor
constexpr double clamp01(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }
} // namespace detail

// Plain value -> normalized 0..1, through the law. Out-of-range values are clamped first.
inline double toNormalized(const ParamInfo& p, double v) {
    v = std::clamp(v, p.min, p.max);
    switch (p.law) {
    case ParamLaw::Log:
        return detail::clamp01(std::log(v / p.min) / std::log(p.max / p.min));
    case ParamLaw::LogZero:
        if (v <= 0.0)
            return 0.0;
        if (v <= detail::kLogZeroFloor)
            return detail::kLogZeroKnee * v / detail::kLogZeroFloor;
        return detail::clamp01(detail::kLogZeroKnee + (1.0 - detail::kLogZeroKnee) *
                                                          std::log(v / detail::kLogZeroFloor) /
                                                          std::log(p.max / detail::kLogZeroFloor));
    case ParamLaw::Linear:
    case ParamLaw::Db:
        break;
    }
    return p.max > p.min ? detail::clamp01((v - p.min) / (p.max - p.min)) : 0.0;
}

// Normalized 0..1 -> plain value, through the law.
inline double fromNormalized(const ParamInfo& p, double n) {
    n = detail::clamp01(n);
    switch (p.law) {
    case ParamLaw::Log:
        return p.min * std::pow(p.max / p.min, n);
    case ParamLaw::LogZero:
        if (n <= detail::kLogZeroKnee)
            return detail::kLogZeroFloor * n / detail::kLogZeroKnee;
        return detail::kLogZeroFloor *
               std::pow(p.max / detail::kLogZeroFloor, (n - detail::kLogZeroKnee) / (1.0 - detail::kLogZeroKnee));
    case ParamLaw::Linear:
    case ParamLaw::Db:
        break;
    }
    return p.min + (p.max - p.min) * n;
}

constexpr bool isStepped(const ParamInfo& p) { return p.kind != ParamKind::Float; }

// True when the host value is the normalized position (see the top of this file).
constexpr bool hostIsNormalized(const ParamInfo& p) {
    return p.kind == ParamKind::Float && p.law != ParamLaw::Linear && (p.flags & PF_HOST_PLAIN) == 0;
}
constexpr double hostMin(const ParamInfo& p) { return hostIsNormalized(p) ? 0.0 : p.min; }
constexpr double hostMax(const ParamInfo& p) { return hostIsNormalized(p) ? 1.0 : p.max; }
inline double hostToPlain(const ParamInfo& p, double h) {
    return hostIsNormalized(p) ? fromNormalized(p, h) : std::clamp(h, p.min, p.max);
}
inline double plainToHost(const ParamInfo& p, double v) {
    return hostIsNormalized(p) ? toNormalized(p, v) : std::clamp(v, p.min, p.max);
}
inline double hostDefault(const ParamInfo& p) { return plainToHost(p, p.def); }

// The value a parameter takes when it is set from outside (host, state, preset, editor): non-finite
// values become the default, values are clamped, stepped kinds (Int, Bool, Enum) are rounded.
inline double sanitizePlain(const ParamInfo& p, double v) {
    if (!std::isfinite(v))
        return p.def;
    v = std::clamp(v, p.min, p.max);
    if (isStepped(p))
        v = std::clamp(std::round(v), p.min, p.max);
    return v;
}

constexpr std::size_t enumCount(const ParamInfo& p) {
    if (p.labels == nullptr || *p.labels == '\0')
        return 0;
    std::size_t n = 1;
    for (const char* c = p.labels; *c != '\0'; ++c)
        n += (*c == '|') ? 1 : 0;
    return n;
}

constexpr std::string_view enumLabel(const ParamInfo& p, std::size_t index) {
    if (p.labels == nullptr)
        return {};
    std::string_view s(p.labels);
    for (std::size_t i = 0; i < index; ++i) {
        const auto bar = s.find('|');
        if (bar == std::string_view::npos)
            return {};
        s.remove_prefix(bar + 1);
    }
    return s.substr(0, s.find('|'));
}

} // namespace spux::inline v1::params
