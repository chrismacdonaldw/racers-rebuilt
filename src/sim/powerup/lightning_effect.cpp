#include "sim/powerup/lightning_effect.hpp"

#include <cstddef>
#include <optional>

#include "core/assert.hpp"
#include "sim/powerup/aim_cones.hpp"
#include "sim/powerup/shot_world.hpp"
#include "sim/powerup/weapon_events.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr float kSegmentShare = 0.25F;
constexpr Vector3 kSourceHeight{.z = 3.0F};

float Share(Milliseconds remaining, Milliseconds total) {
    return static_cast<float>(remaining.count()) / static_cast<float>(total.count());
}

}

void LightningEffect::Start(const PowerupUse& use, PowerupContext& context) {
    RR_ASSERT(context.world != nullptr && context.weapons != nullptr);
    owner_ = OwnerOf(use);
    hazard_ = use.hazard;
    tuning_ = &context.tuning->lightning;
    phase_ = LightningPhase::RampIn;
    timer_ = tuning_->rampTime;
    isHolding_ = false;
    held_ = Milliseconds{0};
    TraceBolt(context);
}

bool LightningEffect::Tick(Milliseconds elapsed, PowerupContext& context) {
    if (phase_ == LightningPhase::Done) {
        return false;
    }
    if (elapsed >= timer_) {
        elapsed -= timer_;
        Advance(context);
    } else {
        timer_ -= elapsed;
    }
    if (phase_ == LightningPhase::Done) {
        return false;
    }
    if (isHolding_) {
        held_ += elapsed;
        if (held_ > tuning_->holdTime) {
            isHolding_ = false;
            held_ = Milliseconds{0};
        }
    }
    TraceBolt(context);
    FindVictim(context);
    return true;
}

bool LightningEffect::Hit(const PowerupHit& /*hit*/, PowerupContext& /*context*/) {
    return false;
}

void LightningEffect::End(PowerupContext& /*context*/) {
    isHolding_ = false;
    phase_ = LightningPhase::Done;
}

EffectProgress LightningEffect::Progress() const {
    return {.phase = static_cast<std::uint8_t>(phase_),
            .phaseTime = timer_,
            .isHoldingRacer = isHolding_};
}

void LightningEffect::Advance(PowerupContext& context) {
    switch (phase_) {
        case LightningPhase::RampIn:
            phase_ = LightningPhase::Sustain;
            timer_ = tuning_->sustainTime;
            break;
        case LightningPhase::Sustain:
            phase_ = LightningPhase::Fade;
            timer_ = tuning_->fadeTime;
            break;
        case LightningPhase::Fade:
        case LightningPhase::Done:
            phase_ = LightningPhase::Done;
            timer_ = Milliseconds{0};
            context.weapons->Add(
                {.kind = WeaponEventKind::Ended,
                 .weapon = PowerupKind::Lightning,
                 .racer = owner_,
                 .position = owner_ ? context.racers[*owner_].Body().motion.position : origin_});
            break;
    }
}

float LightningEffect::SegmentLength() const {
    switch (phase_) {
        case LightningPhase::RampIn:
            return (1.0F - Share(timer_, tuning_->rampTime)) * tuning_->segmentLength;
        case LightningPhase::Fade:
            return Share(timer_, tuning_->fadeTime) * tuning_->segmentLength;
        case LightningPhase::Sustain:
        case LightningPhase::Done:
            break;
    }
    return tuning_->segmentLength;
}

void LightningEffect::TraceBolt(const PowerupContext& context) {
    if (hazard_) {
        const HazardAim& aim = context.hazardAims[*hazard_];
        origin_ = aim.source;
        forward_ = aim.direction;
    } else {
        const BodyMotion& owner = context.racers[*owner_].Body().motion;
        origin_ = owner.position + kSourceHeight;
        forward_ = owner.basis.forward;
    }
    Vector3 direction = forward_;
    float segment = SegmentLength();
    if (isHolding_) {
        const Vector3 toVictim = context.racers[victim_].Body().motion.position - origin_;
        const float radius = context.reaches[victim_].radius;
        const float distance = Length(toVictim);
        direction = Normalized(toVictim, direction);
        segment = (distance > radius ? distance - radius : distance) * kSegmentShare;
    } else if (const std::optional<SegmentHit> hit = CastSolid(
                   ShotsOf(context), origin_,
                   origin_ + (direction * (segment * static_cast<float>(bolt_.size()))))) {
        segment = Length(hit->point - origin_) * kSegmentShare;
    }
    for (std::size_t i = 0; i < bolt_.size(); ++i) {
        bolt_.at(i) = origin_ + (direction * (segment * static_cast<float>(i + 1)));
    }
}

void LightningEffect::FindVictim(PowerupContext& context) {
    if (isHolding_ || phase_ != LightningPhase::Sustain) {
        return;
    }
    for (std::optional<std::size_t> found =
             FindRacerInCone(context.racers, origin_, forward_, tuning_->victimCone);
         found; found = FindRacerInCone(context.racers, origin_, forward_, tuning_->victimCone,
                                        *found + 1)) {
        if (*found != owner_ && !context.racers[*found].Status().isGhost &&
            IsOnBolt(context.reaches[*found])) {
            Strike(static_cast<std::uint8_t>(*found), context);
            return;
        }
    }
}

void LightningEffect::Strike(std::uint8_t racer, PowerupContext& context) {
    Racer& car = context.racers[racer];
    if (!car.IsShielded() && car.IsSpunOut()) {
        return;
    }
    if (HitRacer(context, {.racer = racer, .from = owner_, .by = PowerupKind::Lightning}) !=
        HitOutcome::Struck) {
        return;
    }
    car.LaunchHit(tuning_->hitStrength);
    victim_ = racer;
    isHolding_ = true;
    held_ = Milliseconds{0};
}

bool LightningEffect::IsOnBolt(const BoundingSphere& reach) const {
    Vector3 from = origin_;
    for (const Vector3& to : bolt_) {
        if (Touches(from, to, reach)) {
            return true;
        }
        from = to;
    }
    return false;
}

}
