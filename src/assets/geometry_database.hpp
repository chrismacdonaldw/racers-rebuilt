#pragma once

#include <cstddef>
#include <span>

#include "assets/geometry_asset.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// Positions come out multiplied by the model scale.
[[nodiscard]] Result<GeometryAsset> ParseGeometry(std::span<const std::byte> bytes);

}
