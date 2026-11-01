#pragma once

#include <span>

#include "assets/event_table.hpp"
#include "render/render_view.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

struct CarLook {
    float scale = 1.0F;
    render::VertexColorTransform colors;
};

[[nodiscard]] render::VertexColorTransform ToVertexColors(
    const assets::ColorTransformResource& transform);

/// `tints` are the track's color transforms, which racer snapshots index.
void LookCars(const sim::RenderSnapshot& snapshot,
              std::span<const assets::ColorTransformResource> tints, std::span<CarLook> looks);

}
