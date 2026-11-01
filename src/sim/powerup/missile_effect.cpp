#include "sim/powerup/missile_effect.hpp"

#include <array>
#include <optional>

#include "core/assert.hpp"
#include "sim/physics/rigid_body.hpp"
#include "sim/powerup/aim_cones.hpp"
#include "sim/powerup/ballistic_flight.hpp"
#include "sim/powerup/shot_world.hpp"
#include "sim/powerup/weapon_events.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr BallisticSpec kMissile{
    .speed = 170.0F, .gravity = -32.176F, .lifetime = Milliseconds{5500}};
constexpr std::array kArmTimes{Milliseconds{933}, Milliseconds{1200}, Milliseconds{1333}};
constexpr Vector3 kLaunchHeight{.z = 4.0F};
constexpr Vector3 kAimHeight{.z = 5.0F};
constexpr float kRangeWithoutTarget = 500.0F;
constexpr std::array kSideOffsets{150.0F, 0.0F, -150.0F};
constexpr float kSpinTurns = 2.0F;
constexpr float kSpinRatePerMs = 0.007F;

std::optional<std::size_t> NthRacerAhead(std::span<const Racer> racers, const Vector3& origin,
                                         const Vector3& facing, std::size_t n) {
    std::optional<std::size_t> found = FindRacerInCone(racers, origin, facing, kMissileCone);
    for (std::size_t i = 0; i < n && found; ++i) {
        found = FindRacerInCone(racers, origin, facing, kMissileCone, *found + 1);
    }
    return found;
}

Vector3 AimWithoutTarget(const Vector3& position, const Vector3& facing, std::uint8_t shot) {
    const Vector3 right{.x = facing.y, .y = -facing.x};
    return position + (facing * kRangeWithoutTarget) + kAimHeight + (right * kSideOffsets.at(shot));
}

}

void MissileEffect::Start(const PowerupUse& use, PowerupContext& context) {
    RR_ASSERT(context.world != nullptr && context.checkpoints != nullptr &&
              context.progress != nullptr && context.weapons != nullptr);
    owner_ = use.racer;
    shot_ = use.shot;
    phase_ = ShotPhase::Armed;
    armed_ = kArmTimes.at(shot_);
    carried_ = context.racers[owner_].Pose();
    context.weapons->Add({.kind = WeaponEventKind::Fired,
                          .weapon = PowerupKind::Missile,
                          .racer = owner_,
                          .position = carried_.position});
}

bool MissileEffect::Tick(Milliseconds elapsed, PowerupContext& context) {
    if (phase_ == ShotPhase::Armed) {
        if (elapsed < armed_) {
            armed_ -= elapsed;
            carried_ = context.racers[owner_].Pose();
            return true;
        }
        armed_ = Milliseconds{0};
        Launch(context);
    }
    return phase_ == ShotPhase::Flying && Fly(elapsed, context);
}

bool MissileEffect::Hit(const PowerupHit& /*hit*/, PowerupContext& /*context*/) {
    return false;
}

void MissileEffect::End(PowerupContext& /*context*/) {
    phase_ = ShotPhase::Done;
}

EffectProgress MissileEffect::Progress() const {
    return {.phase = static_cast<std::uint8_t>(phase_),
            .phaseTime = phase_ == ShotPhase::Armed ? armed_ : flight_.Remaining()};
}

Transform MissileEffect::Pose() const {
    if (phase_ == ShotPhase::Armed) {
        return carried_;
    }
    return {.position = flight_.Position(),
            .rotation = ToQuaternion(OrthonormalBasis(flight_.Direction(), {.z = 1.0F}))};
}

void MissileEffect::Launch(const PowerupContext& context) {
    const Racer& owner = context.racers[owner_];
    const Vector3& position = owner.Body().motion.position;
    const Vector3& facing = owner.Body().motion.basis.forward;
    const Vector3 start = position + kLaunchHeight;
    const std::optional<std::size_t> target =
        NthRacerAhead(context.racers, position, facing, shot_);
    BallisticFlight aim;
    if (target) {
        aim.LaunchAtRacer(kMissile, owner, start, context.racers[*target]);
    } else {
        aim.LaunchAtPoint(kMissile, owner, start, AimWithoutTarget(position, facing, shot_));
    }
    std::optional<std::size_t> gate = context.progress->NextGate(*context.checkpoints, owner_);
    if (gate && Dot(context.checkpoints->Checkpoints()[*gate].normal, facing) > 0.0F) {
        gate.reset();
    }
    const Vector3 velocity = aim.Velocity();
    flight_.Launch({.start = start,
                    .direction = Normalized({.x = velocity.x, .y = velocity.y}, facing),
                    .speed = aim.Speed(),
                    .snapSpeed = Length(velocity),
                    .lifetime = kMissile.lifetime,
                    .target = target,
                    .pathGate = gate});
    phase_ = ShotPhase::Flying;
}

bool MissileEffect::Fly(Milliseconds elapsed, PowerupContext& context) {
    flight_.Steer(elapsed, context.racers, owner_, *context.checkpoints, *context.world);
    const FlightState state =
        flight_.Step(elapsed, context.racers, *context.world, ShotsOf(context));
    const ShotOutcome outcome = ResolveShot(context, {.weapon = PowerupKind::Missile,
                                                      .owner = owner_,
                                                      .state = state,
                                                      .position = flight_.Position(),
                                                      .hitNormal = flight_.HitNormal(),
                                                      .target = flight_.Target()});
    if (outcome.turnedBackBy) {
        flight_.TurnBack(owner_);
        owner_ = *outcome.turnedBackBy;
    }
    if (outcome.struck) {
        context.racers[*outcome.struck].StartSpin(kSpinTurns, kSpinRatePerMs);
    }
    if (outcome.isOver) {
        phase_ = ShotPhase::Done;
    }
    return !outcome.isOver;
}

}
