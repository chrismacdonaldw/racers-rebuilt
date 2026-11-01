#include "sim/hazard/ghost.hpp"

#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kBodyNode = 1;
constexpr float kExtraReach = 8.0F;
constexpr float kSpinTurns = 1.0F;
constexpr float kSpinRatePerMs = 0.01F;
constexpr Vector3 kHopDirection{.z = 1.0F};
constexpr float kHopImpulse = 150.0F;

}

void Ghost::Tick(HazardContext& context) {
    if (entity) {
        reach.centre = context.entities->NodeWorld(*entity, kBodyNode).position;
    }
}

void Ghost::Touch(HazardContext& context) const {
    if (!entity) {
        return;
    }
    for (std::size_t i = 0; i < context.racers.size(); ++i) {
        Racer& racer = context.racers[i];
        if (!Touches(reach, context.reaches[i]) || racer.IsSpinning() || racer.IsStopped() ||
            racer.IsShielded()) {
            continue;
        }
        racer.StartSpin(kSpinTurns, kSpinRatePerMs);
        racer.StartSpinOut();
        racer.Stop();
        racer.Push(kHopDirection, kHopImpulse);
        racer.ReactToHit();
    }
}

Ghost LoadGhost(const TrackEntities& entities) {
    Ghost ghost{.entity = entities.Find("ghostly")};
    if (ghost.entity) {
        ghost.reach.radius = entities.Entity(*ghost.entity).radius + kExtraReach;
    }
    return ghost;
}

}
