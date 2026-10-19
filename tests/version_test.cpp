#include <doctest.h>
#include <spux/Version.hpp>

#include <type_traits>

// Everything lives in the inline namespace spux::v1 [L1, L13]: a name is reachable as spux::x and as
// spux::v1::x, and the symbol is mangled with v1.
static_assert(std::is_same_v<decltype(spux::kVersionMajor), decltype(spux::v1::kVersionMajor)>);

TEST_CASE("version constants match the macros") {
    CHECK(spux::kVersionMajor == SPUX_VERSION_MAJOR);
    CHECK(spux::v1::kVersionMinor == SPUX_VERSION_MINOR);
    CHECK(spux::kVersionPatch == SPUX_VERSION_PATCH);
}

TEST_CASE("spux::v1 is an inline namespace") {
    // Qualified lookup through the outer name must find the v1 member.
    CHECK(&spux::kVersionMajor == &spux::v1::kVersionMajor);
}
