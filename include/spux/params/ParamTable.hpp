// The parameter table and the X-macro helpers (decision P2), from Plumeflow `params.def` + `Params`.
//
// The plugin's `.def` file has one row per parameter, Plumeflow's columns plus module and smoothing:
//
//   SPUX_PARAM(id, key, name, kind, min, max, law, default, flags, labels, module, smoothing)
//     kind       Float | Int | Bool | Enum   (Enum: labels split on '|', index 0 at min)
//     law        Linear | Log | LogZero | Db (ParamInfo.hpp)
//     flags      PF_AUTO | PF_MODULATABLE | PF_STEPPED | PF_PER_VOICE | PF_HOST_PLAIN, or 0
//     smoothing  a smoothing policy type (spux/core/Smoothing.hpp: NoSmoothing, LinearRamp<>,
//                OnePole<0.02>, Slew<10.0>, or the plugin's own), used only by SPUX_PARAM_SMOOTHER
// Ids ascend; never renumber an id or rename a key (state and presets use them), only append.
//
// The plugin expands the rows where it needs them (nothing is generated it does not ask for):
//
//   using namespace spux::params;                       // for the flags column
//   inline constexpr ParamInfo kParamInfos[] = {
//   #define SPUX_PARAM SPUX_PARAM_INFO
//   #include "MyParams.def"
//   #undef SPUX_PARAM
//   };
//   inline constexpr ParamTable kParams{kParamInfos};
//   static_assert(kParams.valid());
//
//   enum ParamId : std::uint32_t {                     // P_cutoff = 3001, ...
//   #define SPUX_PARAM SPUX_PARAM_ENUM
//   #include "MyParams.def"
//   #undef SPUX_PARAM
//   };
//
//   struct Smoothers {                                 // one spux::Smoothed<policy> member per key
//   #define SPUX_PARAM SPUX_PARAM_SMOOTHER
//   #include "MyParams.def"
//   #undef SPUX_PARAM
//   };
//
// SPUX_PARAM_COUNT_ONE expands to `+1`, for a count: `constexpr std::size_t kCount = 0 SPUX_PARAM...;`.
// The smoothing column names a type as written; the scope that expands SPUX_PARAM_SMOOTHER must see it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

#include "spux/core/Smoothing.hpp"
#include "spux/params/ParamInfo.hpp"

// Define SPUX_PARAM as an alias of a helper (`#define SPUX_PARAM SPUX_PARAM_INFO`): the row's arguments are
// then split once, by the helper itself, on every preprocessor. A forwarding `#define SPUX_PARAM(...)
// SPUX_PARAM_INFO(__VA_ARGS__)` works too: MSVC's traditional preprocessor passes a forwarded __VA_ARGS__ on as
// one argument, and the helpers expand it once more before splitting it.
#define SPUX_PP_EXPAND(x) x
#define SPUX_PARAM_INFO(...) SPUX_PP_EXPAND(SPUX_PARAM_INFO_(__VA_ARGS__))
#define SPUX_PARAM_ENUM(...) SPUX_PP_EXPAND(SPUX_PARAM_ENUM_(__VA_ARGS__))
#define SPUX_PARAM_SMOOTHER(...) SPUX_PP_EXPAND(SPUX_PARAM_SMOOTHER_(__VA_ARGS__))
#define SPUX_PARAM_INFO_(id, key, name, kind, mn, mx, law, df, flags, labels, module, smoothing)                       \
    ::spux::params::ParamInfo{(id),                                                                                    \
                              #key,                                                                                    \
                              (name),                                                                                  \
                              ::spux::params::ParamKind::kind,                                                         \
                              static_cast<double>(mn),                                                                 \
                              static_cast<double>(mx),                                                                 \
                              ::spux::params::ParamLaw::law,                                                           \
                              static_cast<double>(df),                                                                 \
                              static_cast<std::uint32_t>(flags),                                                       \
                              (labels),                                                                                \
                              (module)},
#define SPUX_PARAM_ENUM_(id, key, ...) P_##key = (id),
#define SPUX_PARAM_COUNT_ONE(...) +1
#define SPUX_PARAM_SMOOTHER_(id, key, name, kind, mn, mx, law, df, flags, labels, module, smoothing)                   \
    ::spux::Smoothed<smoothing, float> key{};

namespace spux::inline v1::params {

inline constexpr std::size_t kNoParam = std::numeric_limits<std::size_t>::max();

// A view of the plugin's ParamInfo array: lookups by id (binary search; ids ascend) and by key.
class ParamTable {
public:
    constexpr ParamTable() = default;
    constexpr explicit ParamTable(std::span<const ParamInfo> infos) : infos_(infos) {}

    [[nodiscard]] constexpr std::size_t size() const { return infos_.size(); }
    [[nodiscard]] constexpr const ParamInfo& operator[](std::size_t index) const { return infos_[index]; }
    [[nodiscard]] constexpr std::span<const ParamInfo> all() const { return infos_; }
    [[nodiscard]] constexpr auto begin() const { return infos_.begin(); }
    [[nodiscard]] constexpr auto end() const { return infos_.end(); }

    // Index of a parameter id, or kNoParam.
    [[nodiscard]] constexpr std::size_t indexOf(std::uint32_t id) const {
        std::size_t lo = 0, hi = infos_.size();
        while (lo < hi) {
            const std::size_t mid = lo + (hi - lo) / 2;
            if (infos_[mid].id < id)
                lo = mid + 1;
            else
                hi = mid;
        }
        return (lo < infos_.size() && infos_[lo].id == id) ? lo : kNoParam;
    }
    // Index of a key (exact match), or kNoParam.
    [[nodiscard]] constexpr std::size_t indexOfKey(std::string_view key) const {
        for (std::size_t i = 0; i < infos_.size(); ++i)
            if (key == infos_[i].key)
                return i;
        return kNoParam;
    }
    [[nodiscard]] constexpr const ParamInfo* find(std::uint32_t id) const {
        const std::size_t i = indexOf(id);
        return i == kNoParam ? nullptr : &infos_[i];
    }
    [[nodiscard]] constexpr const ParamInfo* findByKey(std::string_view key) const {
        const std::size_t i = indexOfKey(key);
        return i == kNoParam ? nullptr : &infos_[i];
    }

    // The table's own rules: ids strictly ascending, keys non-empty and unique, min <= default <= max,
    // Log laws with min > 0, Enum label counts that match the range. For a static_assert.
    [[nodiscard]] constexpr bool valid() const {
        for (std::size_t i = 0; i < infos_.size(); ++i) {
            const ParamInfo& p = infos_[i];
            if (i > 0 && !(infos_[i - 1].id < p.id))
                return false;
            if (p.key == nullptr || *p.key == '\0' || p.name == nullptr || p.labels == nullptr || p.module == nullptr)
                return false;
            if (!(p.min <= p.def && p.def <= p.max))
                return false;
            if (p.law == ParamLaw::Log && !(p.min > 0.0))
                return false;
            if (p.law == ParamLaw::LogZero && !(p.min >= 0.0 && p.max > detail::kLogZeroFloor))
                return false;
            const std::size_t labels = enumCount(p);
            if (labels != 0 && (p.kind != ParamKind::Enum || static_cast<double>(labels) != p.max - p.min + 1.0))
                return false;
            for (std::size_t j = 0; j < i; ++j)
                if (std::string_view(infos_[j].key) == p.key)
                    return false;
        }
        return true;
    }

private:
    std::span<const ParamInfo> infos_{};
};

} // namespace spux::inline v1::params
