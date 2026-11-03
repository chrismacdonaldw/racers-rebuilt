#pragma once

#include "sim/physics/body_state.hpp"

namespace rr::sim {

void UpdateFacing(BodyState& body, float slewRatePerMs, int dtMs);

void UpdateDerivedMotion(BodyState& body);

}
