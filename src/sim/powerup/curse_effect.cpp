#include "sim/powerup/curse_effect.hpp"

#include "core/assert.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr float kTriggerRadius = 10.0F;
constexpr Milliseconds kActive{15'000};
constexpr Milliseconds kFade{1000};
constexpr Milliseconds kCurse{10'000};
constexpr Vector3 kGroundUp{.z = 1.0F};
constexpr Vector3 kFacingWithoutGate{.x = 1.0F};

Quaternion FacingAcross(const Vector3& direction, const Vector3& up) {
    return ToQuaternion(OrthonormalBasis(Cross(up, direction), up));
}

}

void CurseEffect::Start(const PowerupUse& use, PowerupContext& context) {
    RR_ASSERT(context.world != nullptr);
    owner_ = OwnerOf(use);
    level_ = use.level;
    phase_ = TrapPhase::Dropping;
    remaining_ = Milliseconds{0};
    if (use.hazard) {
        const HazardAim& aim = context.hazardAims[*use.hazard];
        position_ = aim.target;
        rotation_ = FacingAcross(-aim.direction, kGroundUp);
    } else {
        position_ = context.racers[use.racer].Body().motion.position;
    }
    modelPosition_ = position_;
}

bool CurseEffect::Tick(Milliseconds elapsed, PowerupContext& context) {
    const Trap curse{
        .position = position_, .radius = kTriggerRadius, .owner = owner_, .level = level_};
    if (phase_ != TrapPhase::Dropping &&
        TouchRacers(context, curse, [](Racer& racer, std::uint8_t /*index*/) {
            if (!racer.Status().isCursed) {
                racer.AttachCurse(kCurse);
            }
            return false;
        })) {
        phase_ = TrapPhase::Done;
    } else if (RunsOut(remaining_, elapsed)) {
        if (phase_ == TrapPhase::Dropping) {
            Drop(context);
        } else {
            Advance();
        }
    }
    return phase_ != TrapPhase::Done;
}

bool CurseEffect::Hit(const PowerupHit& /*hit*/, PowerupContext& /*context*/) {
    return false;
}

void CurseEffect::End(PowerupContext& /*context*/) {
    phase_ = TrapPhase::Done;
}

EffectProgress CurseEffect::Progress() const {
    return {.phase = static_cast<std::uint8_t>(phase_), .phaseTime = remaining_};
}

void CurseEffect::Drop(PowerupContext& context) {
    const DropPoint drop = owner_ ? DropUnder(context.racers[*owner_], *context.world)
                                  : DropPoint{.position = position_};
    if (owner_) {
        const std::optional<std::size_t> gate =
            context.progress->NextGate(*context.checkpoints, *owner_);
        rotation_ = FacingAcross(
            gate ? context.checkpoints->Checkpoints()[*gate].normal : kFacingWithoutGate,
            drop.normal);
    }
    position_ = drop.position;
    modelPosition_ = drop.position + (drop.normal * kCurseModelHeight);
    phase_ = TrapPhase::Armed;
    remaining_ = kActive;
}

void CurseEffect::Advance() {
    if (phase_ == TrapPhase::Armed) {
        phase_ = TrapPhase::Fading;
        remaining_ = kFade;
    } else {
        phase_ = TrapPhase::Done;
    }
}

}
