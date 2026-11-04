#include "sim/powerup/grapple_effect.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>

#include "core/assert.hpp"
#include "sim/powerup/aim_cones.hpp"
#include "sim/powerup/shot_world.hpp"
#include "sim/powerup/weapon_events.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr Vector3 kLaunchHeight{.z = 5.0F};
constexpr Vector3 kRopeHeight{.z = 3.0F};
constexpr Vector3 kHookHeight{.z = 2.0F};
constexpr float kSnapDistanceSquared = 2.0F;
constexpr float kRetractedDistanceSquared = 36.0F;
constexpr std::size_t kOwnerForceSlot = 1;
constexpr std::size_t kVictimForceSlot = 0;

float Seconds(Milliseconds time) {
    return static_cast<float>(time.count()) / 1000.0F;
}

/// Returns true once `current` is within reach of `target` or has passed it.
bool MoveToward(Vector3& current, const Vector3& target, float reachSquared, float step) {
    const Vector3 delta = target - current;
    if (Dot(delta, delta) < reachSquared) {
        return true;
    }
    const Vector3 direction = Normalized(delta, {});
    current += direction * step;
    return Dot(target - current, direction) <= 0.0F;
}

}

void GrappleEffect::Start(const PowerupUse& use, PowerupContext& context) {
    RR_ASSERT(context.world != nullptr && context.weapons != nullptr);
    owner_ = use.racer;
    tuning_ = &context.tuning->grapple;
    hasTouched_ = false;
    tension_ = 0.0F;
    isSnapped_ = false;
    isPulling_ = false;
    const Racer& owner = context.racers[owner_];
    const Vector3& position = owner.Body().motion.position;
    const Vector3& facing = owner.Body().motion.basis.forward;
    const Vector3 start = position + kLaunchHeight;
    std::optional<std::size_t> target =
        FindFarthestRacerInCone(context.racers, position, facing, tuning_->farCone);
    if (!target) {
        target = FindNearestRacerInCone(context.racers, position, facing, tuning_->nearCone);
    }
    if (target) {
        flight_.LaunchAtRacer(tuning_->hook, owner, start, context.racers[*target]);
    } else {
        const std::optional<Vector3> point =
            FindNearestTargetPoint(context.targets, position, facing, tuning_->pointCone);
        flight_.LaunchAtPoint(tuning_->hook, owner, start,
                              point.value_or(start + (facing * tuning_->range)));
    }
    phase_ = GrapplePhase::Flying;
    timer_ = tuning_->hook.lifetime;
    context.weapons->Add({.kind = WeaponEventKind::Fired,
                          .weapon = PowerupKind::GrapplingHook,
                          .racer = owner_,
                          .position = position});
}

bool GrappleEffect::Tick(Milliseconds elapsed, PowerupContext& context) {
    if (phase_ == GrapplePhase::Done) {
        return false;
    }
    if (elapsed >= timer_) {
        elapsed -= timer_;
        timer_ = Milliseconds{0};
        TimeOut(context);
    } else {
        timer_ -= elapsed;
    }
    switch (phase_) {
        case GrapplePhase::Flying:
            Fly(elapsed, context);
            break;
        case GrapplePhase::Pulling:
            Pull(elapsed, context);
            break;
        case GrapplePhase::Retracting:
            Retract(elapsed, context);
            break;
        case GrapplePhase::Done:
            break;
    }
    return phase_ != GrapplePhase::Done;
}

bool GrappleEffect::Hit(const PowerupHit& /*hit*/, PowerupContext& /*context*/) {
    return false;
}

void GrappleEffect::End(PowerupContext& context) {
    EndPull(context);
    phase_ = GrapplePhase::Done;
}

EffectProgress GrappleEffect::Progress() const {
    return {.phase = static_cast<std::uint8_t>(phase_), .phaseTime = timer_};
}

Transform GrappleEffect::Pose() const {
    return {.position = HookPosition()};
}

void GrappleEffect::Fly(Milliseconds elapsed, PowerupContext& context) {
    if (flight_.Step(elapsed, ShotsOf(context)) != FlightState::Flying) {
        Release(flight_.Position(), context);
        return;
    }
    if (const std::optional<std::size_t> touched = FindTouchedRacer(
            context, {.centre = flight_.Position(), .radius = kShotTouchRadius}, owner_)) {
        Hook(static_cast<std::uint8_t>(*touched), context);
    }
}

void GrappleEffect::Hook(std::uint8_t victim, PowerupContext& context) {
    victim_ = victim;
    hasTouched_ = true;
    if (HitRacer(context, {.racer = victim, .from = owner_, .by = PowerupKind::GrapplingHook}) !=
        HitOutcome::Struck) {
        Release(flight_.Position(), context);
        return;
    }
    hook_ = flight_.Position();
    phase_ = GrapplePhase::Pulling;
    timer_ = tuning_->pullTime;
    HoldOrRelease(context);
}

void GrappleEffect::Pull(Milliseconds elapsed, PowerupContext& context) {
    const float seconds = Seconds(elapsed);
    const Vector3 target = context.racers[victim_].Body().motion.position + kHookHeight;
    if (!isSnapped_) {
        isSnapped_ = MoveToward(hook_, target, kSnapDistanceSquared, tuning_->chaseSpeed * seconds);
    }
    if (isSnapped_) {
        hook_ = target;
    }
    tension_ = std::min(tension_ + (tuning_->tensionPerSecond * seconds), 1.0F);
    if (tension_ >= 1.0F) {
        const Vector3 rope = context.racers[owner_].Body().motion.position + kRopeHeight;
        if (const std::optional<SegmentHit> hit = CastSolid(ShotsOf(context), rope, hook_)) {
            Release(hit->point, context);
            return;
        }
    }
    HoldOrRelease(context);
}

void GrappleEffect::HoldOrRelease(PowerupContext& context) {
    Racer& owner = context.racers[owner_];
    Racer& victim = context.racers[victim_];
    const Vector3 pull = victim.Body().motion.position - owner.Body().motion.position;
    if (victim.IsProtected() || Dot(pull, pull) < tuning_->releaseDistanceSquared ||
        Dot(pull, owner.Body().motion.basis.forward) < 0.0F) {
        Release(victim.Body().motion.position, context);
        return;
    }
    const Vector3 force = Normalized(pull, {}) * tuning_->pullForce;
    owner.StartExternalForce(kOwnerForceSlot, force);
    victim.StartExternalForce(kVictimForceSlot, -force);
    isPulling_ = true;
}

void GrappleEffect::Release(const Vector3& at, PowerupContext& context) {
    EndPull(context);
    context.weapons->Add({.kind = hasTouched_ ? WeaponEventKind::Ended : WeaponEventKind::Landed,
                          .weapon = PowerupKind::GrapplingHook,
                          .racer = owner_,
                          .position = at});
    phase_ = GrapplePhase::Retracting;
    timer_ = tuning_->retractTime;
    hook_ = at;
}

void GrappleEffect::Retract(Milliseconds elapsed, const PowerupContext& context) {
    const Vector3 rope = context.racers[owner_].Body().motion.position + kRopeHeight;
    if (MoveToward(hook_, rope, kRetractedDistanceSquared,
                   tuning_->retractSpeed * Seconds(elapsed))) {
        phase_ = GrapplePhase::Done;
    }
}

void GrappleEffect::EndPull(PowerupContext& context) {
    if (!isPulling_) {
        return;
    }
    context.racers[owner_].EndExternalForce(kOwnerForceSlot);
    context.racers[victim_].EndExternalForce(kVictimForceSlot);
    isPulling_ = false;
}

void GrappleEffect::TimeOut(PowerupContext& context) {
    if (phase_ == GrapplePhase::Retracting) {
        phase_ = GrapplePhase::Done;
    } else {
        Release(HookPosition(), context);
    }
}

Vector3 GrappleEffect::HookPosition() const {
    return phase_ == GrapplePhase::Flying ? flight_.Position() : hook_;
}

}
