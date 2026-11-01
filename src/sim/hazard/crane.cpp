#include "sim/hazard/crane.hpp"

#include "sim/hazard/swing_strike.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kBallNode = 3;
constexpr float kBallRadius = 3.0F;
constexpr float kSpinTurns = 1.0F;
constexpr float kSpinRatePerMs = 0.01F;
constexpr StrikeWindows kWindows{
    .strike = {{{.after = 150.0F, .before = 180.0F}, {.after = 0.0F, .before = 30.0F}}},
    .rearm = {{{.after = 60.0F, .before = 120.0F}, {.after = 210.0F, .before = 270.0F}}}};

}

void Crane::Tick(HazardContext& context) {
    if (!entity) {
        return;
    }
    ball = context.entities->NodeWorld(*entity, kBallNode).position;
    if (Swing(kWindows, context.entities->Frame(*entity), isArmed)) {
        context.events->FireStartAndEnd(kCraneSwingEvent, ball);
    }
}

void Crane::Touch(HazardContext& context) const {
    if (!entity) {
        return;
    }
    const BoundingSphere sphere{.centre = ball, .radius = kBallRadius};
    for (std::size_t i = 0; i < context.racers.size(); ++i) {
        Racer& racer = context.racers[i];
        if (Touches(sphere, context.reaches[i]) && !racer.IsSpinning() && !racer.IsStopped()) {
            racer.StartSpin(kSpinTurns, kSpinRatePerMs);
            context.events->FireStartAndEnd(kCraneHitEvent, racer.Pose().position);
        }
    }
}

Crane LoadCrane(const TrackEntities& entities) {
    return {.entity = entities.Find("crane")};
}

}
