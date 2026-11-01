#pragma once

#include "core/vector3.hpp"
#include "sim/physics/racer.hpp"
#include "sim/race/random.hpp"

namespace rr::sim {

/// Runs after the contact's impulse, with the racers in the order `RacerContacts` reports them.
void ApplyContactEffects(Racer& first, Racer& second, const Vector3& point, Random& random);

}
