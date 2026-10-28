#pragma once

#include "sim/physics/body_state.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/physics/driver_input.hpp"

namespace rr::sim {

/// Positive radii turn left. Zero means straight.
[[nodiscard]] float TurnRadiusFor(float steering);

[[nodiscard]] bool CanStartDrift(const BodyState& body);

void ApplyDriveCommand(BodyState& body, const BodyConstants& constants, const DriveCommand& command,
                       const StatMultipliers& stats);

}
