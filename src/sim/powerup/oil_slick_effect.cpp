#include "sim/powerup/oil_slick_effect.hpp"

#include "core/assert.hpp"
#include "sim/powerup/weapon_events.hpp"

namespace rr::sim {
namespace {

constexpr float kTriggerRadius = 3.0F;
constexpr Milliseconds kLifetime{10'000};
constexpr float kSpinTurns = 1.0F;
constexpr float kSpinRate = 0.007F;
constexpr float kSpinAlign = 1.0F;

bool Slip(Racer& racer, std::uint8_t index, PowerupContext& context) {
    if (racer.Status().isHalted) {
        return false;
    }
    racer.StartSpin(kSpinTurns, kSpinRate, kSpinAlign);
    racer.ReactToHit();
    context.weapons->Add({.kind = WeaponEventKind::Struck,
                          .weapon = PowerupKind::OilSlick,
                          .racer = index,
                          .position = racer.Body().motion.position});
    return true;
}

}

void OilSlickEffect::Start(const PowerupUse& use, PowerupContext& context) {
    RR_ASSERT(context.world != nullptr && context.weapons != nullptr);
    owner_ = use.racer;
    phase_ = TrapPhase::Dropping;
    remaining_ = Milliseconds{0};
    position_ = context.racers[owner_].Body().motion.position;
}

bool OilSlickEffect::Tick(Milliseconds elapsed, PowerupContext& context) {
    const Trap slick{.position = position_, .radius = kTriggerRadius, .owner = owner_};
    if (phase_ == TrapPhase::Armed &&
        TouchRacers(context, slick, [&](Racer& racer, std::uint8_t index) {
            return Slip(racer, index, context);
        })) {
        phase_ = TrapPhase::Done;
    } else if (RunsOut(remaining_, elapsed)) {
        if (phase_ == TrapPhase::Dropping) {
            Drop(context);
        } else {
            phase_ = TrapPhase::Done;
        }
    }
    return phase_ != TrapPhase::Done;
}

bool OilSlickEffect::Hit(const PowerupHit& /*hit*/, PowerupContext& /*context*/) {
    return false;
}

void OilSlickEffect::End(PowerupContext& /*context*/) {
    phase_ = TrapPhase::Done;
}

EffectProgress OilSlickEffect::Progress() const {
    return {.phase = static_cast<std::uint8_t>(phase_), .phaseTime = remaining_};
}

void OilSlickEffect::Drop(PowerupContext& context) {
    Racer& owner = context.racers[owner_];
    position_ = DropUnder(owner, *context.world).position;
    KickOwnerOnDrop(owner);
    phase_ = TrapPhase::Armed;
    remaining_ = kLifetime;
    context.weapons->Add({.kind = WeaponEventKind::Fired,
                          .weapon = PowerupKind::OilSlick,
                          .racer = owner_,
                          .position = position_});
}

}
