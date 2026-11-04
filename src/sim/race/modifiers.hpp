#pragma once

#include <optional>

#include "sim/powerup/powerup_kind.hpp"

namespace rr::sim {

/// Port rules for every racer, beside the retail `Cheats`.
struct Modifiers {
    float gravityScale = 1.0F;
    float speedScale = 1.0F;
    std::optional<PowerupKind> onlyPowerup;
    bool hasChaosBricks = false;

    friend bool operator==(const Modifiers&, const Modifiers&) = default;
};

}
