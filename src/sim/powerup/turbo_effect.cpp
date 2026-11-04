#include "sim/powerup/turbo_effect.hpp"

#include <optional>

#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr float kFlameBehind = 2.0F;
constexpr float kFlameAbove = 3.0F;

Transform FlamePose(const Racer& racer) {
    const BodyMotion& motion = racer.Body().motion;
    const Basis& basis = motion.basis;
    return {.position = motion.position - (basis.forward * kFlameBehind) + (basis.up * kFlameAbove),
            .rotation = ToQuaternion(OrthonormalBasis(basis.side, basis.up))};
}

}

void TurboEffect::Start(const PowerupUse& use, PowerupContext& context) {
    racer_ = use.racer;
    Racer& racer = context.racers[racer_];
    racer.React(VoiceReaction::Positive);
    racer.StartTurbo(context.tuning->turbo.levels.at(use.level));
    turbo_ = racer.TurboCount();
    Follow(racer);
}

bool TurboEffect::Tick(Milliseconds /*elapsed*/, PowerupContext& context) {
    const Racer& racer = context.racers[racer_];
    if (!IsRunning(racer)) {
        return false;
    }
    Follow(racer);
    return true;
}

bool TurboEffect::Hit(const PowerupHit& /*hit*/, PowerupContext& /*context*/) {
    return false;
}

void TurboEffect::End(PowerupContext& context) {
    Racer& racer = context.racers[racer_];
    if (IsRunning(racer)) {
        racer.CancelTurbo();
    }
}

bool TurboEffect::IsRunning(const Racer& racer) const {
    return racer.TurboCount() == turbo_ && racer.ActiveTurbo().has_value();
}

void TurboEffect::Follow(const Racer& racer) {
    if (const std::optional<Turbo>& turbo = racer.ActiveTurbo()) {
        progress_ = {.phase = static_cast<std::uint8_t>(turbo->phase),
                     .phaseTime = Milliseconds{turbo->phaseMs}};
    }
    pose_ = FlamePose(racer);
}

}
