#include "sim/powerup/warp_effect.hpp"

#include "core/assert.hpp"
#include "sim/physics/collision_query.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kStartingTime{500};
constexpr Milliseconds kActiveTime{1500};
constexpr float kWalkPerMs = 0.6F;
constexpr Vector3 kPortalHeight{.z = 6.0F};
constexpr Vector3 kDropFrom{.z = 5.0F};
constexpr Vector3 kDropTo{.z = -50.0F};
constexpr Vector3 kLandingHeight{.z = 5.0F};
constexpr Vector3 kHeadingWithoutGate{.x = 1.0F};

float Share(Milliseconds part, Milliseconds whole) {
    return static_cast<float>(part.count()) / static_cast<float>(whole.count());
}

Vector3 GroundBelow(const CollisionQuery& world, const Vector3& point) {
    const std::optional<SegmentHit> hit = CastSolid(world, point + kDropFrom, point + kDropTo);
    return (hit ? hit->point : point) + kLandingHeight;
}

}

void WarpEffect::Start(const PowerupUse& use, PowerupContext& context) {
    RR_ASSERT(use.hazard || (context.checkpoints != nullptr && context.progress != nullptr));
    racer_ = use.racer;
    aim_ = use.hazard ? std::optional(context.hazardAims[*use.hazard]) : std::nullopt;
    Racer& racer = context.racers[racer_];
    phase_ = racer.BeginWarp() ? WarpPhase::Starting : WarpPhase::Done;
    remaining_ = kStartingTime;
    from_ = racer.Body().motion.position;
    pose_ = racer.Pose();
    pose_.position = pose_.position + kPortalHeight;
    if (aim_ && phase_ == WarpPhase::Starting) {
        Advance(context);
        pose_ = racer.Pose();
    }
}

bool WarpEffect::Tick(Milliseconds elapsed, PowerupContext& context) {
    if (phase_ == WarpPhase::Done) {
        return false;
    }
    Racer& racer = context.racers[racer_];
    if (phase_ == WarpPhase::Active && aim_) {
        Lerp(context, elapsed, aim_->target);
    } else if (phase_ == WarpPhase::Active) {
        const float distance = static_cast<float>(elapsed.count()) * kWalkPerMs;
        racer.MoveWarp(context.checkpoints->WalkAhead(racer.Body().motion.position, distance,
                                                      NextGate(context).value_or(0)));
    }
    if (elapsed >= remaining_) {
        Advance(context);
    } else {
        remaining_ -= elapsed;
    }
    pose_ = racer.Pose();
    if (phase_ == WarpPhase::Starting) {
        pose_.position = pose_.position + kPortalHeight;
    }
    return phase_ != WarpPhase::Done;
}

bool WarpEffect::Hit(const PowerupHit& /*hit*/, PowerupContext& /*context*/) {
    return false;
}

void WarpEffect::End(PowerupContext& context) {
    if (phase_ == WarpPhase::Starting) {
        context.racers[racer_].CancelWarp();
    } else if (phase_ == WarpPhase::Active) {
        Land(context);
    }
    phase_ = WarpPhase::Done;
}

EffectProgress WarpEffect::Progress() const {
    return {.phase = static_cast<std::uint8_t>(phase_), .phaseTime = remaining_};
}

void WarpEffect::Advance(PowerupContext& context) {
    if (phase_ == WarpPhase::Starting) {
        context.racers[racer_].EnterWarp();
        phase_ = WarpPhase::Active;
        remaining_ = kActiveTime;
        return;
    }
    Land(context);
    phase_ = WarpPhase::Done;
}

void WarpEffect::Lerp(PowerupContext& context, Milliseconds elapsed, const Vector3& to) {
    const float left =
        Share(remaining_ > elapsed ? remaining_ - elapsed : Milliseconds{0}, kActiveTime);
    context.racers[racer_].MoveWarp({.x = to.x + ((from_.x - to.x) * left),
                                     .y = to.y + ((from_.y - to.y) * left),
                                     .z = to.x + ((from_.z - to.x) * left)});
}

void WarpEffect::Land(PowerupContext& context) {
    Racer& racer = context.racers[racer_];
    if (aim_) {
        racer.LandWarp(GroundBelow(*context.world, aim_->target), aim_->direction);
        return;
    }
    const std::optional<std::size_t> gate = NextGate(context);
    const Vector3 heading =
        gate ? -context.checkpoints->Checkpoints()[*gate].normal : kHeadingWithoutGate;
    racer.LandWarp(GroundBelow(*context.world, racer.Body().motion.position), heading);
}

std::optional<std::size_t> WarpEffect::NextGate(const PowerupContext& context) const {
    return context.progress->NextGate(*context.checkpoints, racer_);
}

}
