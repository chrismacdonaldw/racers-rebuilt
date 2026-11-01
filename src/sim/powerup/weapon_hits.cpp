#include "sim/powerup/weapon_hits.hpp"

#include "core/assert.hpp"
#include "sim/powerup/brick_field.hpp"
#include "sim/powerup/explosions.hpp"
#include "sim/powerup/powerup_system.hpp"
#include "sim/powerup/shield_effect.hpp"
#include "sim/powerup/weapon_events.hpp"

namespace rr::sim {
namespace {

constexpr float kShotTouchRadius = 2.5F;
constexpr float kScarNormalZ = 0.7071F;

}

std::optional<std::size_t> FindRacerInCone(std::span<const Racer> racers, const Vector3& origin,
                                           const Vector3& direction, const AimCone& cone,
                                           std::size_t first) {
    for (std::size_t i = first; i < racers.size(); ++i) {
        const Vector3 delta = racers[i].Body().motion.position - origin;
        const float distanceSquared = Dot(delta, delta);
        if (distanceSquared >= cone.minDistanceSquared &&
            distanceSquared <= cone.maxDistanceSquared &&
            Dot(direction, Normalized(delta, {})) >= cone.cosine) {
            return i;
        }
    }
    return std::nullopt;
}

bool Touches(const BoundingSphere& a, const BoundingSphere& b) {
    const Vector3 delta = a.centre - b.centre;
    const float reach = a.radius + b.radius;
    return Dot(delta, delta) < reach * reach;
}

std::optional<std::size_t> FindTouchedRacer(const PowerupContext& context,
                                            const BoundingSphere& sphere, std::size_t owner) {
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
        return TurnsShotsBack(victim) ? HitOutcome::TurnedBack : HitOutcome::Absorbed;
    }
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
