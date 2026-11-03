#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "assets/geometry_asset.hpp"
#include "core/result.hpp"

namespace rr::assets {

enum class GdbVertices : std::uint8_t { Lit, Colored };

/// GDB keeps either normals (`Lit`) or colours (`Colored`). Positions are stored divided by scale.
[[nodiscard]] Result<std::vector<std::byte>> WriteGeometry(const GeometryAsset& geometry,
                                                           GdbVertices vertices);

}
