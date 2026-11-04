#pragma once

#include "assets/geometry_asset.hpp"
#include "assets/piece_library.hpp"

namespace rr::assets {

inline constexpr int kHighestPlate = 0x3F;

/// `model` sits on its cells from the origin in game units, bottom at z = 0.
[[nodiscard]] PieceShape DeriveShape(const GeometryAsset& model);

/// Whether `DeriveShape` can read `model`: finite, inside the build grid from the origin, and no
/// higher than a cell's top can hold.
[[nodiscard]] bool IsOnBuildGrid(const GeometryAsset& model);

}
