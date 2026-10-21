// Modulation routes: a source drives a target by a signed depth. A plugin with one target per LFO or envelope
// uses one route per source; a plugin whose sources reach many targets (drag a source onto a knob to add a route,
// set its depth there) uses more routes. The matrix is the same list either way.
//
// Source and target are ids the plugin defines; a source's current value (an LFO, an envelope, a wheel) comes
// in as an array indexed by source id, per voice where the source is per voice. The depth is in the target's
// normalized units at full source value, so amountFor() is added to the target's normalized value and
// effective() clamps the sum to 0..1. How a depth parameter maps to the signed number is the plugin's
// (a 0..1 knob with 0.5 at the middle, or a percentage). The UI reads the same numbers through ModulationHost.
// Fixed size, no allocation.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace spux::inline v1::dsp {

inline constexpr std::uint32_t kNoModId = 0xFFFFFFFFu;

struct ModRoute {
    std::uint32_t source = kNoModId, target = kNoModId;
    float depth = 0.0f;
    [[nodiscard]] bool active() const { return source != kNoModId && target != kNoModId && depth != 0.0f; }
};

template <std::size_t MaxRoutes> class ModMatrix {
public:
    [[nodiscard]] static constexpr std::size_t size() { return MaxRoutes; }

    void set(std::size_t i, std::uint32_t source, std::uint32_t target, float depth) {
        routes_[i] = {source, target, depth};
    }
    void setDepth(std::size_t i, float depth) { routes_[i].depth = depth; }
    void clear(std::size_t i) { routes_[i] = {}; }
    void clear() { routes_ = {}; }
    [[nodiscard]] const ModRoute& route(std::size_t i) const { return routes_[i]; }

    // The first route without a source and target: where a new one goes. size() when every route is in use.
    [[nodiscard]] std::size_t freeRoute() const {
        for (std::size_t i = 0; i < MaxRoutes; ++i)
            if (routes_[i].source == kNoModId && routes_[i].target == kNoModId)
                return i;
        return MaxRoutes;
    }
    // The route from `source` to `target`, or size().
    [[nodiscard]] std::size_t find(std::uint32_t source, std::uint32_t target) const {
        for (std::size_t i = 0; i < MaxRoutes; ++i)
            if (routes_[i].source == source && routes_[i].target == target)
                return i;
        return MaxRoutes;
    }
    [[nodiscard]] int routesInto(std::uint32_t target) const {
        int n = 0;
        for (const ModRoute& r : routes_)
            n += r.active() && r.target == target;
        return n;
    }

    // The sum of depth x source value over the routes into `target` (a source id beyond the array counts as 0).
    [[nodiscard]] float amountFor(std::uint32_t target, std::span<const float> sources) const {
        float sum = 0.0f;
        for (const ModRoute& r : routes_)
            if (r.active() && r.target == target && r.source < sources.size())
                sum += r.depth * sources[r.source];
        return sum;
    }
    [[nodiscard]] float effective(std::uint32_t target, float baseNorm, std::span<const float> sources) const {
        return std::clamp(baseNorm + amountFor(target, sources), 0.0f, 1.0f);
    }

private:
    std::array<ModRoute, MaxRoutes> routes_{};
};

} // namespace spux::inline v1::dsp
