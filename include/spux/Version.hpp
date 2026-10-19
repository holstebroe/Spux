// Version and the inline namespace every spux symbol lives in (decision L1, L13): two plugins built
// against different spux versions can share one host process without clashing symbols.
#pragma once

#define SPUX_VERSION_MAJOR 0
#define SPUX_VERSION_MINOR 1
#define SPUX_VERSION_PATCH 0

namespace spux {
// Bump the inline namespace with every major version (spux::v2 ...); the old one is never reused.
inline namespace v1 {

inline constexpr int kVersionMajor = SPUX_VERSION_MAJOR;
inline constexpr int kVersionMinor = SPUX_VERSION_MINOR;
inline constexpr int kVersionPatch = SPUX_VERSION_PATCH;

} // namespace v1
} // namespace spux
