#pragma once

#include "sim/physics/body_state.hpp"

namespace rr::sim {

void UpdateFacing(BodyState& body, int dtMs);

void UpdateDerivedMotion(BodyState& body);

}
