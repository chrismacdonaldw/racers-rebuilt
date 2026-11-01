#include "sim/powerup/dynamite_effect.hpp"

#include <cstddef>

#include "core/assert.hpp"
#include "core/quaternion.hpp"
#include "sim/powerup/aim_cones.hpp"
#include "sim/powerup/dropped_trap.hpp"
#include "sim/powerup/explosions.hpp"
#include "sim/powerup/weapon_events.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr BallisticSpec kDynamite{
    .speed = 40.0F, .gravity = -32.176F, .lifetime = Milliseconds{3000}, .leadsOnlyBehind = true};
constexpr Vector3 kLaunchHeight{.z = 5.0F};
constexpr float kRangeWithoutTarget = -90.0F;
constexpr Milliseconds kFuse{5000};
constexpr Milliseconds kBlastInterval{500};
constexpr float kTumblePerMs = 0.012F;
constexpr std::uint32_t kScatterSpan = 13;
constexpr int kScatterOffset = 6;

}

void DynamiteEffect::Start(const PowerupUse& use, PowerupContext& context) {
    RR_ASSERT(context.world != nullptr && context.spikes != nullptr && context.weapons != nullptr);
    owner_ = use.racer;
    const Racer& owner = context.racers[owner_];
    landing_ = owner.Body().motion.position;
    target_.reset();
    if (const std::optional<std::size_t> target = FindRacerInCone(
            context.racers, landing_, owner.Body().motion.basis.forward * -1.0F, kRacerAheadCone)) {
        target_ = static_cast<std::uint8_t>(*target);
    }
    phase_ = DynamitePhase::Lit;
    remaining_ = Milliseconds{0};
    tumble_ = 0.0F;
}

bool DynamiteEffect::Tick(Milliseconds elapsed, PowerupContext& context) {
    if (phase_ == DynamitePhase::Flying && HasLanded(elapsed, context)) {
        landing_ = flight_.Position();
        Blast(context, landing_);
        context.weapons->Add({.kind = WeaponEventKind::Landed,
                              .weapon = PowerupKind::Dynamite,
                              .racer = owner_,
                              .position = landing_});
        phase_ = DynamitePhase::Blasting;
        remaining_ = kBlastInterval;
    }
    tumble_ += kTumblePerMs * static_cast<float>(elapsed.count());
    if (RunsOut(remaining_, elapsed)) {
        Advance(context);
    }
    return phase_ != DynamitePhase::Done;
}

bool DynamiteEffect::Hit(const PowerupHit& /*hit*/, PowerupContext& /*context*/) {
    return false;
}

void DynamiteEffect::End(PowerupContext& /*context*/) {
    phase_ = DynamitePhase::Done;
}

EffectProgress DynamiteEffect::Progress() const {
    return {.phase = static_cast<std::uint8_t>(phase_), .phaseTime = remaining_};
}

Transform DynamiteEffect::Pose() const {
    return {.position = phase_ == DynamitePhase::Flying ? flight_.Position() : landing_,
            .rotation = FromAxisAngle({.y = 1.0F}, tumble_)};
}

void DynamiteEffect::Launch(const PowerupContext& context) {
    const Racer& owner = context.racers[owner_];
    const Vector3 start = owner.Body().motion.position + kLaunchHeight;
    if (target_) {
        flight_.LaunchAtRacer(kDynamite, owner, start, context.racers[*target_]);
    } else {
        flight_.LaunchAtPoint(kDynamite, owner, start,
                              start + (owner.Body().motion.basis.forward * kRangeWithoutTarget));
    }
    phase_ = DynamitePhase::Flying;
    remaining_ = kFuse;
}

bool DynamiteEffect::HasLanded(Milliseconds elapsed, const PowerupContext& context) {
    if (flight_.Step(elapsed, *context.world) != FlightState::Flying) {
        return true;
    }
    return FindTouchedRacer(context, {.centre = flight_.Position(), .radius = kShotTouchRadius},
                            owner_)
        .has_value();
}

void DynamiteEffect::Advance(PowerupContext& context) {
    switch (phase_) {
        case DynamitePhase::Lit:
            Launch(context);
            return;
        case DynamitePhase::Flying:
            Blast(context, flight_.Position());
            phase_ = DynamitePhase::Done;
            return;
        case DynamitePhase::Blasting:
            Blast(context, Scattered(context));
            phase_ = DynamitePhase::LastBlast;
            remaining_ = kBlastInterval;
            return;
        case DynamitePhase::LastBlast:
            Blast(context, Scattered(context));
            phase_ = DynamitePhase::Done;
            return;
        case DynamitePhase::Done:
            return;
    }
}

void DynamiteEffect::Blast(PowerupContext& context, const Vector3& at) const {
    context.spikes->Spawn(at, true, owner_);
}

Vector3 DynamiteEffect::Scattered(PowerupContext& context) const {
    const auto scatter = [&context] {
        return static_cast<float>(static_cast<int>(context.random->Below(kScatterSpan)) -
                                  kScatterOffset);
    };
    const float x = scatter();
    const float y = scatter();
    return landing_ + Vector3{.x = x, .y = y};
}

}
