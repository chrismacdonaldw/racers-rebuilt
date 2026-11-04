#pragma once

#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"

namespace rr::sim {

class Racer;

struct PlayerCheats {
    bool isInvincible = false;
    bool hasBottomlessBrick = false;
    bool keepsFullWhite = false;
    bool hasBrickMagnet = false;
    bool hasLongTurbo = false;
    bool hasPerfectStart = false;
    bool hasFreeWarp = false;
    Milliseconds headStart{0};

    friend bool operator==(const PlayerCheats&, const PlayerCheats&) = default;
};

void GiveRacerCheats(Racer& racer, const PlayerCheats& cheats);

[[nodiscard]] BoundingSphere PickupReach(const BoundingSphere& reach, const PlayerCheats& cheats);

}
