#pragma once

#include <optional>

#include "sim/physics/body_state.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/physics/driver_input.hpp"

namespace rr::sim {

/// In units per s² before the acceleration multiplier, doubled while braking against motion.
inline constexpr float kPedalThrust = 54.0F;
inline constexpr float kBrakeThrustScale = 2.0F;

/// Positive radii turn left. Zero means straight.
[[nodiscard]] float TurnRadiusFor(float steering);

/// Inverts `TurnRadiusFor`, past full lock for radii under 40.
[[nodiscard]] float SteeringFor(float radius);

/// Steering assist widens any turn radius under this, in units.
[[nodiscard]] float GripLimit(const BodyState& body, float gravity);

[[nodiscard]] bool CanStartDrift(const BodyState& body);

/// `turboThrust` replaces the pedal thrust, in units per s² before the acceleration multiplier.
void ApplyDriveCommand(BodyState& body, const BodyConstants& constants, const DriveCommand& command,
                       const StatMultipliers& stats,
                       std::optional<float> turboThrust = std::nullopt);

}
