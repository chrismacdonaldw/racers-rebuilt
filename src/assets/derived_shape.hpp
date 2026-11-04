#pragma once

#include "assets/geometry_asset.hpp"
#include "assets/piece_library.hpp"

namespace rr::assets {

/// `model` sits on its cells from the origin in game units, bottom at z = 0.
[[nodiscard]] PieceShape DeriveShape(const GeometryAsset& model);

}
