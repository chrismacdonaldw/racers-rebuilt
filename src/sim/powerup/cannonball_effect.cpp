#include "sim/powerup/cannonball_effect.hpp"

#include <optional>

#include "core/assert.hpp"
#include "sim/powerup/aim_cones.hpp"
#include "sim/powerup/shot_world.hpp"
#include "sim/powerup/weapon_events.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr BallisticSpec kCannonball{
    .speed = 180.0F, .gravity = -32.176F, .lifetime = Milliseconds{3000}};
constexpr BallisticSpec kEmplacementCannonball{
    .speed = 30.0F, .gravity = -96.528F, .lifetime = Milliseconds{3000}};
constexpr Vector3 kLaunchHeight{.z = 5.0F};
constexpr float kRangeWithoutTarget = 500.0F;

}

void CannonballEffect::Start(const PowerupUse& use, PowerupContext& context) {
    RR_ASSERT(context.world != nullptr && context.explosions != nullptr &&
              context.weapons != nullptr);
    owner_ = OwnerOf(use);
    const Vector3 from = use.hazard ? LaunchFrom(context.hazardAims[*use.hazard])
                                    : LaunchFrom(context.racers[use.racer], context);
    phase_ = ShotPhase::Flying;
    context.weapons->Add({.kind = WeaponEventKind::Fired,
                          .weapon = PowerupKind::Cannonball,
                          .racer = owner_,
                          .position = from});
}

bool CannonballEffect::Tick(Milliseconds elapsed, PowerupContext& context) {
    if (phase_ != ShotPhase::Flying) {
        return false;
    }
    const FlightState state = flight_.Step(elapsed, ShotsOf(context));
    const ShotOutcome outcome = ResolveShot(context, {.weapon = PowerupKind::Cannonball,
                                                      .owner = owner_,
                                                      .state = state,
                                                      .position = flight_.Position(),
                                                      .hitNormal = flight_.HitNormal()});
    if (outcome.turnedBackBy) {
        flight_.TurnBack(owner_ ? &context.racers[*owner_] : nullptr);
        owner_ = *outcome.turnedBackBy;
    }
    if (outcome.isOver) {
        phase_ = ShotPhase::Done;
    }
    return !outcome.isOver;
}

bool CannonballEffect::Hit(const PowerupHit& /*hit*/, PowerupContext& /*context*/) {
    return false;
}

void CannonballEffect::End(PowerupContext& /*context*/) {
    phase_ = ShotPhase::Done;
}

Vector3 CannonballEffect::LaunchFrom(const Racer& owner, const PowerupContext& context) {
    const Vector3& position = owner.Body().motion.position;
    const Vector3& facing = owner.Body().motion.basis.forward;
    const Vector3 start = position + kLaunchHeight;
    const std::optional<Vector3> point =
        FindNearestTargetPoint(context.targets, position, facing, kTargetPointCone);
    const std::optional<std::size_t> target =
        point ? std::nullopt
              : FindNearestRacerInCone(context.racers, position, facing, kRacerAheadCone);
    if (target) {
        flight_.LaunchAtRacer(kCannonball, owner, start, context.racers[*target]);
    } else {
        flight_.LaunchAtPoint(kCannonball, owner, start,
                              point.value_or(start + (facing * kRangeWithoutTarget)));
    }
    return position;
}

Vector3 CannonballEffect::LaunchFrom(const HazardAim& aim) {
    flight_.LaunchAtPoint(kEmplacementCannonball, aim.source, aim.target);
    return aim.source;
}

EffectProgress CannonballEffect::Progress() const {
    return {.phase = static_cast<std::uint8_t>(phase_), .phaseTime = flight_.Remaining()};
}

}
