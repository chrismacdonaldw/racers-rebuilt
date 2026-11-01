#include "sim/powerup/shield_effect.hpp"

#include <array>
#include <cstddef>
#include <memory>

namespace rr::sim {
namespace {

enum class ShieldPhase : std::uint8_t { Up, Fade };

constexpr std::array kDurations{Milliseconds{4000}, Milliseconds{6000}, Milliseconds{8000},
                                Milliseconds{10'000}};
constexpr Milliseconds kFade{1000};
constexpr float kModelHeight = 2.0F;
constexpr std::uint8_t kDeflectLevel = 2;

}

void ShieldEffect::Start(const PowerupUse& use, PowerupContext& context) {
    racer_ = use.racer;
    duration_ = kDurations.at(use.level);
    progress_ = {};
    Racer& racer = context.racers[racer_];
    racer.RaiseShield(use.level);
    raised_ = racer.Status().shield.raised;
    Follow(racer);
}

bool ShieldEffect::Tick(Milliseconds elapsed, PowerupContext& context) {
    const Racer& racer = context.racers[racer_];
    if (!IsHeld(racer)) {
        return false;
    }
    progress_.phaseTime += elapsed;
    if (progress_.phase == static_cast<std::uint8_t>(ShieldPhase::Up) &&
        progress_.phaseTime >= duration_) {
        progress_ = {.phase = static_cast<std::uint8_t>(ShieldPhase::Fade),
                     .phaseTime = progress_.phaseTime - duration_};
    }
    if (progress_.phase == static_cast<std::uint8_t>(ShieldPhase::Fade) &&
        progress_.phaseTime >= kFade) {
        return false;
    }
    Follow(racer);
    return true;
}

bool ShieldEffect::Hit(const PowerupHit& /*hit*/, PowerupContext& context) {
    Racer& racer = context.racers[racer_];
    if (!IsHeld(racer)) {
        return false;
    }
    racer.AbsorbShieldHit(*context.random);
    return true;
}

void ShieldEffect::End(PowerupContext& context) {
    Racer& racer = context.racers[racer_];
    if (IsHeld(racer)) {
        racer.LowerShield();
    }
}

bool ShieldEffect::IsHeld(const Racer& racer) const {
    return racer.IsShielded() && racer.Status().shield.raised == raised_;
}

void ShieldEffect::Follow(const Racer& racer) {
    pose_ = racer.Pose();
    pose_.position.z += kModelHeight;
}

bool TurnsShotsBack(const Racer& racer) {
    const std::optional<std::uint8_t>& level = racer.Status().shield.level;
    return level && *level >= kDeflectLevel;
}

void AddBlueEffects(EffectFactories& factories) {
    factories.at(static_cast<std::size_t>(PowerupKind::Shield)) = [] {
        return std::make_unique<ShieldEffect>();
    };
}

}
