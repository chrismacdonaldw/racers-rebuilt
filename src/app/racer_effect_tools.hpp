#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "sim/physics/racer.hpp"

namespace rr::app {

enum class RacerEffect : std::uint8_t { Spin, LaunchHit, Hover, Curse, Push, MagnetHold, Ghost };

inline constexpr std::array kRacerEffects{
    RacerEffect::Spin, RacerEffect::LaunchHit,  RacerEffect::Hover, RacerEffect::Curse,
    RacerEffect::Push, RacerEffect::MagnetHold, RacerEffect::Ghost};

/// Names the button for `effect`, which ends a lasting effect that `racer` already has.
[[nodiscard]] std::string RacerEffectLabel(RacerEffect effect, const sim::Racer& racer);

/// Hits `racer` the way the power-up that causes `effect` would, or ends the lasting effect.
void ApplyRacerEffect(RacerEffect effect, sim::Racer& racer);

}
