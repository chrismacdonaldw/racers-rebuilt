#include "sim/powerup/warp_effect.hpp"

#include <cstddef>
#include <optional>
#include <span>

#include "core/assert.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/powerup/warp_landing.hpp"

namespace rr::sim {
namespace {

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
    const WarpTuning& tuning = context.tuning->warp;
    pace_ = aim_ ? tuning.pace : PaceFor(rules_, tuning.pace, IsLeading(context));
    Racer& racer = context.racers[racer_];
    phase_ = racer.BeginWarp() ? WarpPhase::Starting : WarpPhase::Done;
    remaining_ = tuning.startingTime;
    from_ = racer.Body().motion.position;
    pose_ = racer.Pose();
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
        const float distance = static_cast<float>(elapsed.count()) * pace_.walkPerMs;
        racer.MoveWarp(context.checkpoints->WalkAhead(racer.Body().motion.position, distance,
                                                      NextGate(context).value_or(0)));
    }
    if (elapsed >= remaining_) {
        Advance(context);
    } else {
        remaining_ -= elapsed;
    }
    pose_ = racer.Pose();
    return phase_ != WarpPhase::Done;
}

bool WarpEffect::Hit(const PowerupHit& /*hit*/, PowerupContext& /*context*/) {
    return false;
}

void WarpEffect::End(PowerupContext& context) {
    if (!rules_.endsWhenTakenBack) {
        phase_ = WarpPhase::Done;
        return;
    }
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
        context.racers[racer_].EnterWarp(pace_.routeSpeed);
        phase_ = WarpPhase::Active;
        remaining_ = context.tuning->warp.activeTime;
        return;
    }
    Land(context);
    phase_ = WarpPhase::Done;
}

void WarpEffect::Lerp(PowerupContext& context, Milliseconds elapsed, const Vector3& to) {
    const float left = Share(remaining_ > elapsed ? remaining_ - elapsed : Milliseconds{0},
                             context.tuning->warp.activeTime);
    context.racers[racer_].MoveWarp({.x = to.x + ((from_.x - to.x) * left),
                                     .y = to.y + ((from_.y - to.y) * left),
                                     .z = to.x + ((from_.z - to.z) * left)});
}

void WarpEffect::Land(PowerupContext& context) {
    Racer& racer = context.racers[racer_];
    if (aim_) {
        racer.LandWarp(GroundBelow(*context.world, aim_->target), aim_->direction);
        return;
    }
    const Vector3 at = GroundBelow(*context.world, racer.Body().motion.position);
    racer.LandWarp(at, LandingHeading(context, at - kLandingHeight), pace_.launch);
}

Vector3 WarpEffect::LandingHeading(const PowerupContext& context, const Vector3& ground) const {
    const std::optional<std::size_t> gate = NextGate(context);
    if (rules_.aimsLanding && !context.racers[racer_].Route() &&
        !context.checkpoints->Checkpoints().empty()) {
        return ClearLandingHeading(*context.world, *context.checkpoints, ground, gate.value_or(0));
    }
    return gate ? -context.checkpoints->Checkpoints()[*gate].normal : kHeadingWithoutGate;
}

bool WarpEffect::IsLeading(const PowerupContext& context) const {
    const std::span<const std::size_t> standings = context.progress->Standings();
    return standings.size() > 1 && standings.front() == racer_;
}

std::optional<std::size_t> WarpEffect::NextGate(const PowerupContext& context) const {
    return context.progress->NextGate(*context.checkpoints, racer_);
}

}
