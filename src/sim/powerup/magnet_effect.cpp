#include "sim/powerup/magnet_effect.hpp"

#include "core/assert.hpp"
#include "sim/physics/rigid_body.hpp"
#include "sim/powerup/dropped_trap.hpp"
#include "sim/powerup/weapon_events.hpp"

namespace rr::sim {
namespace {

constexpr Vector3 kModelHeight{.z = 30.0F};
constexpr Vector3 kWorldUp{.z = 1.0F};

std::optional<Quaternion> TiltToward(const Vector3& model, const Vector3& racer) {
    const Vector3 up = Normalized(model - racer, {});
    const Vector3 forward = Normalized(Cross(Cross(kWorldUp, up), up), {});
    if (Dot(forward, forward) == 0.0F || Dot(up, up) == 0.0F) {
        return std::nullopt;
    }
    return ToQuaternion({.forward = forward, .side = Cross(up, forward), .up = up});
}

}

void MagnetEffect::Start(const PowerupUse& use, PowerupContext& context) {
    RR_ASSERT(context.world != nullptr && context.weapons != nullptr);
    owner_ = use.racer;
    tuning_ = &context.tuning->magnet;
    phase_ = MagnetPhase::Armed;
    remaining_ = tuning_->armed;
    isDropped_ = false;
    dropPoint_ = context.racers[owner_].Body().motion.position;
    heading_ = {.x = 1.0F};
    facing_ = {};
    held_.reset();
    pulled_.reset();
    hasStopped_ = false;
    hasLifted_ = false;
}

bool MagnetEffect::Tick(Milliseconds elapsed, PowerupContext& context) {
    if (!isDropped_) {
        Drop(context);
    } else if (TouchRacers(context,
                           {.position = dropPoint_, .radius = tuning_->radius, .owner = owner_},
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
    Face(context);
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
    return {.position = dropPoint_ + kModelHeight, .rotation = facing_};
}

void MagnetEffect::Drop(PowerupContext& context) {
    Racer& owner = context.racers[owner_];
    dropPoint_ = DropUnder(owner, *context.world).position;
    KickOwnerOnDrop(owner);
    isDropped_ = true;
    if (context.checkpoints != nullptr && context.progress != nullptr) {
        const std::optional<std::size_t> last =
            context.progress->OfRacer(owner_).laps.LastCheckpoint();
        if (last) {
            heading_ = context.checkpoints->Checkpoints()[*last].normal;
        }
    }
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
        remaining_ = tuning_->holding;
    }
    const Vector3 offset = racer.Body().motion.position - dropPoint_;
    if (Dot(offset, offset) > tuning_->grabDistanceSquared) {
        if (!pulled_) {
            pulled_ = index;
        }
    } else if (!held_) {
        held_ = index;
        racer.StartMagnetHold();
        racer.React(VoiceReaction::Negative);
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
    remaining_ = tuning_->fade;
    context.weapons->Add({.kind = WeaponEventKind::Ended,
                          .weapon = PowerupKind::Magnet,
                          .racer = owner_,
                          .position = Pose().position});
}

void MagnetEffect::Hold(Racer& racer, Milliseconds elapsed) {
    const float speed = racer.Body().derived.speed;
    if (speed <= tuning_->stoppedSpeed) {
        hasStopped_ = true;
        if (!racer.Status().isHalted && remaining_ > tuning_->haltedHold) {
            remaining_ = tuning_->haltedHold;
        }
        racer.Halt();
    }
    if (!racer.Route() && hasStopped_ && !hasLifted_) {
        const Vector3 toDrop = dropPoint_ - racer.Body().motion.position;
        racer.Push(Normalized(toDrop, {}), PullFor(elapsed));
        if (speed <= tuning_->stoppedSpeed &&
            Dot(toDrop, toDrop) <= tuning_->liftStopDistanceSquared) {
            hasLifted_ = true;
        }
    }
    if (racer.ActiveTurbo()) {
        racer.CancelTurbo();
    }
}

void MagnetEffect::Face(const PowerupContext& context) {
    const std::optional<std::uint8_t> caught = held_ ? held_ : pulled_;
    const std::optional<Quaternion> tilted =
        caught ? TiltToward(Pose().position, context.racers[*caught].Body().motion.position)
               : std::nullopt;
    if (tilted) {
        facing_ = *tilted;
    } else if (!caught) {
        facing_ = ToQuaternion(OrthonormalBasis(heading_, kWorldUp));
    }
}

float MagnetEffect::PullFor(Milliseconds elapsed) const {
    return static_cast<float>(elapsed.count()) * tuning_->pullPerMs;
}

void MagnetEffect::Pull(Racer& racer, Milliseconds elapsed) const {
    racer.Push(Normalized(dropPoint_ - racer.Body().motion.position, {}), PullFor(elapsed));
}

}
