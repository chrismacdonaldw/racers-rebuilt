#pragma once

#include <span>

#include "app/racer/racer_assets.hpp"
#include "app/racer/racer_draws.hpp"
#include "assets/geometry_asset.hpp"
#include "core/bounding_sphere.hpp"
#include "core/result.hpp"

namespace rr::app {

[[nodiscard]] BoundingSphere ModelBounds(std::span<const assets::GeometryVertex> vertices);

[[nodiscard]] BoundingSphere GroupBounds(std::span<const BoundingSphere> members);

/// In the car's frame from the chassis origin, around the shown parts only.
[[nodiscard]] Result<BoundingSphere> RacerBounds(const RacerAssets& racer, RacerParts shown);

}
