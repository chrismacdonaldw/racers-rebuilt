#include "sim/powerup/weapon_hits.hpp"

#include <algorithm>

#include "core/assert.hpp"
#include "sim/powerup/brick_field.hpp"
#include "sim/powerup/explosions.hpp"
#include "sim/powerup/powerup_system.hpp"
#include "sim/powerup/shield_effect.hpp"
#include "sim/powerup/weapon_events.hpp"

namespace rr::sim {
namespace {

constexpr float kScarNormalZ = 0.7071F;

}

bool Touches(const BoundingSphere& a, const BoundingSphere& b) {
    const Vector3 delta = a.centre - b.centre;
    const float reach = a.radius + b.radius;
    return Dot(delta, delta) < reach * reach;
}

bool Touches(const Vector3& from, const Vector3& to, const BoundingSphere& sphere) {
    const Vector3 segment = to - from;
    const float lengthSquared = Dot(segment, segment);
    const float along =
        lengthSquared > 0.0F
            ? std::clamp(Dot(sphere.centre - from, segment) / lengthSquared, 0.0F, 1.0F)
            : 0.0F;
    return Touches({.centre = from + (segment * along)}, sphere);
}

std::optional<std::size_t> FindTouchedRacer(const PowerupContext& context,
                                            const BoundingSphere& sphere,
                                            std::optional<std::size_t> owner) {
    for (std::size_t i = 0; i < context.reaches.size(); ++i) {
        if (i != owner && !context.racers[i].Status().isGhost &&
            Touches(sphere, context.reaches[i])) {
            return i;
        }
    }
    return std::nullopt;
}

HitOutcome HitRacer(PowerupContext& context, const PowerupHit& hit) {
    RR_ASSERT(context.powerups != nullptr && context.bricks != nullptr &&
              context.weapons != nullptr);
    Racer& victim = context.racers[hit.racer];
    if (context.powerups->Hit(hit, context)) {
        victim.React(VoiceReaction::Positive);
        return TurnsShotsBack(victim) ? HitOutcome::TurnedBack : HitOutcome::Absorbed;
    }
    if (victim.IsInvincible()) {
        return HitOutcome::Absorbed;
    }
    if (hit.from) {
        context.racers[*hit.from].React(VoiceReaction::Positive);
    }
    victim.React(VoiceReaction::Negative);
    victim.ReactToHit();
    const Vector3& at = victim.Body().motion.position;
    context.bricks->DropWhite(context.inventories[hit.racer], at, *context.world);
    context.weapons->Add(
        {.kind = WeaponEventKind::Struck, .weapon = hit.by, .racer = hit.racer, .position = at});
    return HitOutcome::Struck;
}

ShotOutcome ResolveShot(PowerupContext& context, const ShotStep& shot) {
    RR_ASSERT(context.explosions != nullptr);
    if (shot.state == FlightState::HitWorld || shot.state == FlightState::Expired) {
        context.weapons->Add(
            {.kind = WeaponEventKind::Landed, .weapon = shot.weapon, .position = shot.position});
        context.explosions->Spawn(
            shot.position, shot.state == FlightState::HitWorld && shot.hitNormal.z > kScarNormalZ,
            shot.owner);
        return {.isOver = true};
    }
    const std::optional<std::size_t> touched =
        shot.state == FlightState::ReachedTarget
            ? shot.target
            : FindTouchedRacer(context, {.centre = shot.position, .radius = kShotTouchRadius},
                               shot.owner);
    if (!touched) {
        return {};
    }
    const auto victim = static_cast<std::uint8_t>(*touched);
    const HitOutcome outcome =
        HitRacer(context, {.racer = victim, .from = shot.owner, .by = shot.weapon});
    if (outcome == HitOutcome::TurnedBack) {
        return {.turnedBackBy = victim};
    }
    context.explosions->Spawn(context.racers[victim].Body().motion.position, false, shot.owner);
    return {.isOver = true,
            .struck = outcome == HitOutcome::Struck ? std::optional(victim) : std::nullopt};
}

}
