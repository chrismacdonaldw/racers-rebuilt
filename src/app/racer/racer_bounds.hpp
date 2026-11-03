#pragma once

#include <span>
#include <string_view>

#include "app/racer/racer_assets.hpp"
#include "app/racer/racer_draws.hpp"
#include "assets/geometry_asset.hpp"
#include "core/bounding_sphere.hpp"
#include "core/result.hpp"

namespace rr::app {

[[nodiscard]] Result<const assets::GeometryAsset*> FindRacerModel(const RacerAssets& racer,
                                                                  std::string_view name);

/// Around the wheel model's forward animation, in the car's frame.
[[nodiscard]] Result<BoundingSphere> WheelBounds(const RacerAssets& racer);

[[nodiscard]] BoundingSphere ModelBounds(std::span<const assets::GeometryVertex> vertices);

[[nodiscard]] BoundingSphere GroupBounds(std::span<const BoundingSphere> members);

/// In the car's frame from the chassis origin, around the shown parts only.
[[nodiscard]] Result<BoundingSphere> RacerBounds(const RacerAssets& racer, RacerParts shown);

}
