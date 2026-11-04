#pragma once

#include <span>
#include <string>

#include "assets/geometry_asset.hpp"
#include "core/vector3.hpp"
#include "sim/car_build/mesh_primitives.hpp"

namespace rr::sim {

/// Turns and moves `model` the way the retail piece would be, then shifts it by `offset`.
void AppendPlacedModel(assets::GeometryAsset& geometry, const assets::GeometryAsset& model,
                       const PieceInstance& instance, const Vector3& offset,
                       std::span<const std::string> materials);

}
