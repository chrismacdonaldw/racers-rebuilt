#include "sim/hazard/ghost.hpp"

#include <algorithm>
#include <cstdint>

#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr float kExtraReach = 8.0F;
constexpr float kSpinTurns = 1.0F;
constexpr float kSpinRatePerMs = 0.01F;
constexpr Vector3 kHopDirection{.z = 1.0F};
constexpr float kHopImpulse = 150.0F;
constexpr Milliseconds kNearCheck{4000};
constexpr float kNearDistance = 60.0F;
constexpr std::int32_t kNearSound = 3013;
constexpr std::int32_t kHitSound = 3015;

bool IsAnyRacerNear(const HazardContext& context, const Vector3& centre) {
    return std::ranges::any_of(context.racers, [&](const Racer& racer) {
        const Vector3 offset = racer.Body().motion.position - centre;
        return Dot(offset, offset) <= kNearDistance * kNearDistance;
    });
}

}

void Ghost::Tick(HazardContext& context) {
    if (!entity) {
        return;
    }
    reach.centre = context.entities->NodeWorld(*entity, kGhostBodyNode).position;
    nearWait += context.elapsed;
    if (nearWait < kNearCheck) {
        return;
    }
    nearWait = Milliseconds{0};
    if (IsAnyRacerNear(context, reach.centre)) {
        context.sounds->push_back({.soundId = kNearSound, .position = reach.centre});
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
        racer.React(VoiceReaction::Negative);
        context.sounds->push_back({.soundId = kHitSound, .position = racer.Body().motion.position});
    }
}

Ghost LoadGhost(const TrackEntities& entities, ModelSwitches& models) {
    models.SetHidden(models.Add("gtrail"), true);
    Ghost ghost{.entity = entities.Find("ghostly")};
    if (ghost.entity) {
        ghost.reach.radius = entities.Entity(*ghost.entity).radius + kExtraReach;
    }
    return ghost;
}

}
