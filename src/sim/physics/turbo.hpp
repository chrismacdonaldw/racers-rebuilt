#pragma once

#include <cstdint>

#include "sim/physics/body_state.hpp"
#include "sim/physics/chassis.hpp"

namespace rr::sim {

enum class TurboLevel : std::uint8_t { Weak, Strong, Hover };

enum class TurboPhase : std::uint8_t { Ignition, Boost, Fade };

inline constexpr int kTurboIgnitionMs = 400;

struct Turbo {
    TurboLevel level = TurboLevel::Weak;
    TurboPhase phase = TurboPhase::Ignition;
    int phaseMs = 0;
    bool hasTouchedWall = false;
};

[[nodiscard]] Turbo StartTurbo(BodyState& body, const BodyConstants& constants,
                               const StatMultipliers& stats, TurboLevel level);

void EndTurbo(BodyState& body, const BodyConstants& constants, const StatMultipliers& stats);

/// Returns false once the turbo ends.
[[nodiscard]] bool AdvanceTurbo(Turbo& turbo, int dtMs);

void FadeTurbo(Turbo& turbo);

[[nodiscard]] bool CanFadeEarly(const Turbo& turbo);

/// In units per s², before the acceleration multiplier.
[[nodiscard]] float TurboThrust(const Turbo& turbo);

}
