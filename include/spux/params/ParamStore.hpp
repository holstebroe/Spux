// Host-facing parameter values, lock-free. One atomic double per
// parameter in the host domain (ParamInfo.hpp), indexed like the table. Written by the host's threads,
// the audio thread and the editor, read by all of them; every access is relaxed: a parameter value
// carries no other data with it. The version counter goes up on every write, so an editor redraws only
// when something changed (guidelines 7, immediate mode).
#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "spux/params/ParamInfo.hpp"
#include "spux/params/ParamTable.hpp"

namespace spux::inline v1::params {

template <std::size_t N> class ParamStore {
public:
    static constexpr std::size_t kCount = N;
    static_assert(std::atomic<double>::is_always_lock_free, "the parameter store must be lock-free");

    explicit ParamStore(ParamTable table) : table_(table) {
        assert(table.size() == N);
        resetToDefaults();
    }

    [[nodiscard]] const ParamTable& table() const { return table_; }

    void resetToDefaults() {
        for (std::size_t i = 0; i < N; ++i)
            values_[i].store(hostDefault(table_[i]), std::memory_order_relaxed);
        bump();
    }

    // Host domain, stored as given (the host and the editor send values in range).
    void setHost(std::size_t index, double v) {
        values_[index].store(v, std::memory_order_relaxed);
        bump();
    }
    [[nodiscard]] double host(std::size_t index) const { return values_[index].load(std::memory_order_relaxed); }

    // Plain domain. setPlain sanitizes (non-finite -> default, clamped, stepped kinds rounded).
    void setPlain(std::size_t index, double v) {
        setHost(index, plainToHost(table_[index], sanitizePlain(table_[index], v)));
    }
    [[nodiscard]] double plain(std::size_t index) const { return hostToPlain(table_[index], host(index)); }

    // Normalized 0..1 (a control's position).
    [[nodiscard]] double normalized(std::size_t index) const { return toNormalized(table_[index], plain(index)); }

    // Changes with every write; compare with the value seen last.
    [[nodiscard]] std::uint32_t version() const { return version_.load(std::memory_order_relaxed); }

private:
    void bump() { version_.fetch_add(1, std::memory_order_relaxed); }

    ParamTable table_;
    std::array<std::atomic<double>, N> values_{};
    std::atomic<std::uint32_t> version_{0};
};

} // namespace spux::inline v1::params
