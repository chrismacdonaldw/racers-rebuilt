#pragma once

#include "sim/physics/body_state.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/physics/collision_query.hpp"

namespace rr::sim {

void UpdateHoverLift(HoverLift& lift, int dtMs);

/// Leans into a turn of `turnRadius`, up to 45° on the tightest turns and none driving straight.
[[nodiscard]] float HoverBankTarget(float turnRadius);

/// Casts one ray per axle midpoint and holds the car the lift height above the higher hit.
void UpdateHoverContact(BodyState& body, const BodyConstants& constants, int dtMs,
                        const CollisionQuery& world);

}
