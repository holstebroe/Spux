// The test table (test_params.def) expanded the way a plugin does it (spux/params/ParamTable.hpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "spux/params/ParamTable.hpp"

namespace testparams {
using namespace spux::params;
using namespace spux; // the smoothing column names spux policies

inline constexpr ParamInfo kInfos[] = {
#define SPUX_PARAM SPUX_PARAM_INFO
#include "params/test_params.def"
#undef SPUX_PARAM
};
inline constexpr ParamTable kTable{kInfos};

enum ParamId : std::uint32_t {
#define SPUX_PARAM SPUX_PARAM_ENUM
#include "params/test_params.def"
#undef SPUX_PARAM
};

inline constexpr std::size_t kCount = 0
#define SPUX_PARAM SPUX_PARAM_COUNT_ONE
#include "params/test_params.def"
#undef SPUX_PARAM
    ;

struct Smoothers {
#define SPUX_PARAM SPUX_PARAM_SMOOTHER
#include "params/test_params.def"
#undef SPUX_PARAM
};

} // namespace testparams
