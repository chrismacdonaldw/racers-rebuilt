#pragma once

#include <optional>

#include "sim/physics/body_state.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/physics/driver_input.hpp"

namespace rr::sim {

/// Positive radii turn left. Zero means straight.
[[nodiscard]] float TurnRadiusFor(float steering);

[[nodiscard]] bool CanStartDrift(const BodyState& body);

/// `turboThrust` replaces the pedal thrust, in units per s² before the acceleration multiplier.
void ApplyDriveCommand(BodyState& body, const BodyConstants& constants, const DriveCommand& command,
                       const StatMultipliers& stats,
                       std::optional<float> turboThrust = std::nullopt);

}
