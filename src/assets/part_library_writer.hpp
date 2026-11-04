#pragma once

#include <cstddef>
#include <vector>

#include "assets/part_library.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// Pool values round to the GCB's integers: positions as int16, UVs in 1/4096 and normals in 1/127.
[[nodiscard]] Result<std::vector<std::byte>> WritePartLibrary(const PartLibrary& library);

}
