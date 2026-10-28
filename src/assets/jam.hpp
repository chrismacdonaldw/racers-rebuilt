#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "assets/path_index.hpp"
#include "core/result.hpp"

namespace rr::assets {

struct JamFileRange {
    std::uint32_t offset = 0;
    std::uint32_t sizeBytes = 0;
};

/// File entries in `paths` have slots that index `files`.
struct JamIndex {
    PathIndex paths;
    std::vector<JamFileRange> files;
};

/// Every range in the result lies inside `bytes`.
[[nodiscard]] Result<JamIndex> ParseJam(std::span<const std::byte> bytes);

}
