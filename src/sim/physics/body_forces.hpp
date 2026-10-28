#pragma once

#include "sim/physics/body_state.hpp"
#include "sim/physics/chassis.hpp"

namespace rr::sim {

void AccumulateForces(BodyState& body, const BodyConstants& constants);

}
