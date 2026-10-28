#pragma once

#include "core/vector3.hpp"
#include "sim/physics/body_state.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/physics/collision_query.hpp"

namespace rr::sim {

[[nodiscard]] BodyState MakeBody(const BodyConstants& constants, const Vector3& position,
                                 const Basis& basis);

/// `thrust` and `maxSpeed` are in units per s² and units per s, as in the chassis stats.
void SetDrive(BodyState& body, const BodyConstants& constants, float thrust, float maxSpeed);

void SetTurnRadius(BodyState& body, float radius);

void StepBody(BodyState& body, const BodyConstants& constants, int dtMs,
              const CollisionQuery& world);

}
