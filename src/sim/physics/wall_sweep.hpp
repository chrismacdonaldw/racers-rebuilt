#pragma once

#include <optional>

#include "core/vector3.hpp"
#include "sim/physics/body_state.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/surface_touches.hpp"

namespace rr::sim {

struct WallHit {
    Vector3 normal;
    float fraction = 0.0F;
};

/// Each body point starts at its pose in `body.saved`. The deepest hit wins.
[[nodiscard]] std::optional<WallHit> SweepBodyPoints(const BodyState& body,
                                                     const BodyConstants& constants,
                                                     const CollisionQuery& query,
                                                     SurfaceTouches* touches = nullptr);

/// The milliseconds of a `stepMs` step to rerun after a wall hit at `fraction` of it.
[[nodiscard]] int RetryStepMs(int stepMs, float fraction);

void ApplyWallResponse(BodyState& body, const BodyConstants& constants, const Vector3& normal);

}
