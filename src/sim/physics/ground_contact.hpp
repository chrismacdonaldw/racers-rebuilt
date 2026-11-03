#pragma once

#include "sim/physics/body_state.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/physics/collision_query.hpp"

namespace rr::sim {

void PlaceWheels(BodyMotion& motion, const BodyConstants& constants);

void UpdateGroundContact(BodyState& body, const BodyConstants& constants, int dtMs,
                         const CollisionQuery& world);

void ClearContacts(BodyMotion& motion);

/// Ignoring surfaces keeps the road's handling but the real surface's sound, events and entry.
void UpdateWheelSurfaces(BodyMotion& motion, bool ignoresSurfaces);

/// Zero when no wheel is in contact.
[[nodiscard]] float AverageContactGrip(const BodyMotion& motion);

}
