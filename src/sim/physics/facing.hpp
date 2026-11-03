#pragma once

#include "sim/physics/body_state.hpp"
#include "sim/physics/version_rules.hpp"

namespace rr::sim {

void UpdateFacing(BodyState& body, float slewRatePerMs, int dtMs);

void UpdateDerivedMotion(BodyState& body);

/// The original's shortest frame, which gets one `DriftRoll` kick.
inline constexpr float kDriftRollStepMs = 12.0F;

/// How far the facing trails the car while drifting, signed by the turn, as body roll.
[[nodiscard]] float DriftRoll(const BodyState& body, bool isDrifting, const VersionRules& rules);

}
