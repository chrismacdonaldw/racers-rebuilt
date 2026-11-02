#pragma once

#include <vector>

#include "app/quad_shapes.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"
#include "sim/physics/collision_query.hpp"

namespace rr::app {

/// In game space, with `length` along the level part of `forward`.
struct GroundDecal {
    Vector3 centre;
    Vector3 forward;
    float length = 0.0F;
    float width = 0.0F;
};

/// Projects `decal` straight down onto the ground under its centre.
void AppendGroundDecal(const QuadLook& look, const sim::CollisionQuery& ground,
                       const GroundDecal& decal, std::vector<render::QuadDraw>& out);

}
