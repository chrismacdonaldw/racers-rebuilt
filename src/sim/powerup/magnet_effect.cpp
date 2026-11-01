#include "sim/powerup/magnet_effect.hpp"

#include "core/assert.hpp"
#include "sim/powerup/dropped_trap.hpp"
#include "sim/powerup/weapon_events.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kArmed{20'000};
constexpr Milliseconds kHolding{4000};
constexpr Milliseconds kFade{1000};
constexpr Milliseconds kHaltedHold{1000};
constexpr float kTriggerRadius = 10.0F;
constexpr Vector3 kModelHeight{.z = 30.0F};
constexpr float kGrabDistanceSquared = 81.0F;
constexpr float kLiftStopDistanceSquared = 100.0F;
constexpr float kStoppedSpeed = 0.002F;
constexpr float kPullPerMs = 1.0F;

float PullFor(Milliseconds elapsed) {
    return static_cast<float>(elapsed.count()) * kPullPerMs;
}

}

void MagnetEffect::Start(const PowerupUse& use, PowerupContext& context) {
    RR_ASSERT(context.world != nullptr && context.weapons != nullptr);
    owner_ = use.racer;
    phase_ = MagnetPhase::Armed;
    remaining_ = kArmed;
    isDropped_ = false;
    dropPoint_ = context.racers[owner_].Body().motion.position;
    held_.reset();
    pulled_.reset();
    hasStopped_ = false;
    hasLifted_ = false;
}

bool MagnetEffect::Tick(Milliseconds elapsed, PowerupContext& context) {
    if (!isDropped_) {
        Drop(context);
    } else if (TouchRacers(context,
                           {.position = dropPoint_, .radius = kTriggerRadius, .owner = owner_},
                           [&](Racer& racer, std::uint8_t index) {
                               Touch(racer, index, context);
                               return false;
                           })) {
        phase_ = MagnetPhase::Done;
        return false;
    }
    if (RunsOut(remaining_, elapsed)) {
        Expire(context);
    }
    if (held_ && !context.racers[*held_].Status().isMagnetHeld) {
        held_.reset();
    }
    if (held_) {
        Hold(context.racers[*held_], elapsed);
    } else if (pulled_) {
        Pull(context.racers[*pulled_], elapsed);
    }
    pulled_.reset();
    return phase_ != MagnetPhase::Done;
}

bool MagnetEffect::Hit(const PowerupHit& /*hit*/, PowerupContext& /*context*/) {
    return false;
}

void MagnetEffect::End(PowerupContext& context) {
    if (held_ && context.racers[*held_].Status().isMagnetHeld) {
        context.racers[*held_].EndMagnetHold();
    }
    held_.reset();
    phase_ = MagnetPhase::Done;
}

EffectProgress MagnetEffect::Progress() const {
    return {.phase = static_cast<std::uint8_t>(phase_), .phaseTime = remaining_};
}

Transform MagnetEffect::Pose() const {
    return {.position = dropPoint_ + kModelHeight};
}

void MagnetEffect::Drop(PowerupContext& context) {
    Racer& owner = context.racers[owner_];
    dropPoint_ = DropUnder(owner, *context.world).position;
    KickOwnerOnDrop(owner);
    isDropped_ = true;
    context.weapons->Add({.kind = WeaponEventKind::Fired,
                          .weapon = PowerupKind::Magnet,
                          .racer = owner_,
                          .position = dropPoint_});
}

void MagnetEffect::Touch(Racer& racer, std::uint8_t index, PowerupContext& context) {
    if (phase_ == MagnetPhase::Fading) {
        return;
    }
    if (phase_ == MagnetPhase::Armed) {
        phase_ = MagnetPhase::Holding;
        remaining_ = kHolding;
    }
    const Vector3 offset = racer.Body().motion.position - dropPoint_;
    if (Dot(offset, offset) > kGrabDistanceSquared) {
        if (!pulled_) {
            pulled_ = index;
        }
    } else if (!held_) {
        held_ = index;
        racer.StartMagnetHold();
        pulled_.reset();
        context.weapons->Add({.kind = WeaponEventKind::Struck,
                              .weapon = PowerupKind::Magnet,
                              .racer = index,
                              .position = Pose().position});
    }
}

void MagnetEffect::Expire(PowerupContext& context) {
    if (phase_ == MagnetPhase::Fading) {
        phase_ = MagnetPhase::Done;
        return;
    }
    phase_ = MagnetPhase::Fading;
    remaining_ = kFade;
    context.weapons->Add({.kind = WeaponEventKind::Ended,
                          .weapon = PowerupKind::Magnet,
                          .racer = owner_,
                          .position = Pose().position});
}

void MagnetEffect::Hold(Racer& racer, Milliseconds elapsed) {
    const float speed = racer.Body().derived.speed;
    if (speed <= kStoppedSpeed) {
        hasStopped_ = true;
        if (!racer.Status().isHalted && remaining_ > kHaltedHold) {
            remaining_ = kHaltedHold;
        }
        racer.Halt();
    }
    if (!racer.Route() && hasStopped_ && !hasLifted_) {
        const Vector3 toDrop = dropPoint_ - racer.Body().motion.position;
        racer.Push(Normalized(toDrop, {}), PullFor(elapsed));
        if (speed <= kStoppedSpeed && Dot(toDrop, toDrop) <= kLiftStopDistanceSquared) {
            hasLifted_ = true;
        }
    }
    if (racer.ActiveTurbo()) {
        racer.CancelTurbo();
    }
}

void MagnetEffect::Pull(Racer& racer, Milliseconds elapsed) const {
    racer.Push(Normalized(dropPoint_ - racer.Body().motion.position, {}), PullFor(elapsed));
}

}
