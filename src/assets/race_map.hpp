#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>

#include "core/result.hpp"

namespace rr::assets {

struct MapBounds {
    float minX = 0.0F;
    float maxY = 0.0F;
    float maxX = 0.0F;
    float minY = 0.0F;
};

/// `world` is the race file's name for it, such as `local.wdf`.
struct RaceMapFields {
    std::optional<MapBounds> bounds;
    std::optional<std::string> world;
};

[[nodiscard]] Result<RaceMapFields> ReadRaceMapFields(std::span<const std::byte> rab);

}
