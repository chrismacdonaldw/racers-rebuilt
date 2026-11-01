#include "sim/powerup/lightning_effect.hpp"

#include <cstddef>
#include <optional>

#include "core/assert.hpp"
#include "sim/powerup/aim_cones.hpp"
#include "sim/powerup/weapon_events.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kRampTime{500};
constexpr Milliseconds kSustainTime{7000};
constexpr Milliseconds kFadeTime{500};
constexpr Milliseconds kHoldTime{1000};
constexpr float kSegmentLength = 50.0F;
constexpr float kSegmentShare = 0.25F;
constexpr Vector3 kSourceHeight{.z = 3.0F};
constexpr AimCone kVictimCone{
    .minDistanceSquared = 9.0F, .maxDistanceSquared = 40'000.0F, .cosine = 0.5F};

float Share(Milliseconds remaining, Milliseconds total) {
    return static_cast<float>(remaining.count()) / static_cast<float>(total.count());
}

}

void LightningEffect::Start(const PowerupUse& use, PowerupContext& context) {
    RR_ASSERT(context.world != nullptr && context.weapons != nullptr);
    owner_ = use.racer;
    phase_ = LightningPhase::RampIn;
    timer_ = kRampTime;
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
        if (held_ > kHoldTime) {
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
    return {.phase = static_cast<std::uint8_t>(phase_), .phaseTime = timer_};
}

void LightningEffect::Advance(PowerupContext& context) {
    switch (phase_) {
        case LightningPhase::RampIn:
            phase_ = LightningPhase::Sustain;
            timer_ = kSustainTime;
            break;
        case LightningPhase::Sustain:
            phase_ = LightningPhase::Fade;
            timer_ = kFadeTime;
            break;
        case LightningPhase::Fade:
        case LightningPhase::Done:
            phase_ = LightningPhase::Done;
            timer_ = Milliseconds{0};
            context.weapons->Add({.kind = WeaponEventKind::Ended,
                                  .weapon = PowerupKind::Lightning,
                                  .racer = owner_,
                                  .position = context.racers[owner_].Body().motion.position});
            break;
    }
}

float LightningEffect::SegmentLength() const {
    switch (phase_) {
        case LightningPhase::RampIn:
            return (1.0F - Share(timer_, kRampTime)) * kSegmentLength;
        case LightningPhase::Fade:
            return Share(timer_, kFadeTime) * kSegmentLength;
        case LightningPhase::Sustain:
        case LightningPhase::Done:
            break;
    }
    return kSegmentLength;
}

void LightningEffect::TraceBolt(const PowerupContext& context) {
    const BodyMotion& owner = context.racers[owner_].Body().motion;
    origin_ = owner.position + kSourceHeight;
    Vector3 direction = owner.basis.forward;
    float segment = SegmentLength();
    if (isHolding_) {
        const Vector3 toVictim = context.racers[victim_].Body().motion.position - origin_;
        const float radius = context.reaches[victim_].radius;
        const float distance = Length(toVictim);
        direction = Normalized(toVictim, direction);
        segment = (distance > radius ? distance - radius : distance) * kSegmentShare;
    } else if (const std::optional<SegmentHit> hit = CastSolid(
                   *context.world, origin_,
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
    const Vector3& forward = context.racers[owner_].Body().motion.basis.forward;
    for (std::optional<std::size_t> found =
             FindRacerInCone(context.racers, origin_, forward, kVictimCone);
         found;
         found = FindRacerInCone(context.racers, origin_, forward, kVictimCone, *found + 1)) {
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
    car.LaunchHit(1.0F);
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
