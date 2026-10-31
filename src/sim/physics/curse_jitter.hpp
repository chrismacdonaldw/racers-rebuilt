#pragma once

#include "sim/physics/driver_input.hpp"
#include "sim/race/random.hpp"

namespace rr::sim {

struct CurseJitter {
    int countdownMs = 0;
    float throttleOffset = 0.0F;
    float steerOffset = 0.0F;
};

/// Rolls a new jitter from `random` each time the countdown runs out while `isCursed`.
void UpdateCurseJitter(CurseJitter& jitter, int dtMs, bool isCursed, Random& random);

/// Reverses the steering and offsets the throttle.
[[nodiscard]] DriveCommand Cursed(const DriveCommand& command, const CurseJitter& jitter);

}
