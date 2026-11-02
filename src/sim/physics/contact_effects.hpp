#pragma once

#include <span>

#include "sim/physics/racer.hpp"
#include "sim/physics/racer_contacts.hpp"
#include "sim/race/random.hpp"

namespace rr::sim {

/// Runs after the contact's impulse, with the racers in the order `RacerContacts` reports them.
void ApplyContactEffects(std::span<Racer> racers, const RacerContact& contact, Random& random);

}
