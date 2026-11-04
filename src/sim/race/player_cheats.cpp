#include "sim/race/player_cheats.hpp"

#include "sim/physics/racer.hpp"

namespace rr::sim {
namespace {

constexpr int kLongTurboScale = 3;
constexpr float kBrickMagnetScale = 3.0F;

}

void GiveRacerCheats(Racer& racer, const PlayerCheats& cheats) {
    if (cheats.isInvincible) {
        racer.MakeInvincible();
    }
    if (cheats.hasLongTurbo) {
        racer.LengthenTurboBoosts(kLongTurboScale);
    }
    if (cheats.hasPerfectStart) {
        racer.PerfectStarts();
    }
}

BoundingSphere PickupReach(const BoundingSphere& reach, const PlayerCheats& cheats) {
    return cheats.hasBrickMagnet
               ? BoundingSphere{.centre = reach.centre, .radius = reach.radius * kBrickMagnetScale}
               : reach;
}

}
