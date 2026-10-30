#pragma once

#include <cstdint>
#include <vector>

#include "app/racer/racer_assets.hpp"
#include "assets/geometry_asset.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// Draws of one model at its origin, with the skeleton node each draw follows.
struct ModelDraws {
    std::vector<render::MeshDraw> draws;
    std::vector<std::uint16_t> joints;
    float scale = 1.0F;
};

struct RacerDraws {
    ModelDraws wheels;
    ModelDraws body;
    ModelDraws driver;
};

/// `draws` holds one draw per range of `geometry`, in order.
[[nodiscard]] ModelDraws FollowJoints(const assets::GeometryAsset& geometry,
                                      std::vector<render::MeshDraw> draws);

[[nodiscard]] Result<RacerDraws> UploadRacer(render::Renderer& renderer, const RacerAssets& racer);

}
