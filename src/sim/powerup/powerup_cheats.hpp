#pragma once

#include <optional>

#include "assets/brick_layout.hpp"
#include "sim/powerup/powerup_kind.hpp"
#include "sim/race/cheats.hpp"

namespace rr::sim {

[[nodiscard]] std::optional<assets::BrickColour> CheatBrickColour(const Cheats& cheats);

/// What a racer's use fires instead, or nothing when the brick is spent without effect.
[[nodiscard]] std::optional<PowerupUse> CheatUse(const Cheats& cheats, PowerupUse use);

/// The colour and level that fire `kind`. A shield keeps the use's level, and a turbo up to 2.
[[nodiscard]] PowerupUse OnlyUse(PowerupKind kind, PowerupUse use);

}
