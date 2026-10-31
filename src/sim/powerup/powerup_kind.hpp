#pragma once

#include <cstddef>
#include <cstdint>

#include "assets/brick_layout.hpp"

namespace rr::sim {

inline constexpr std::uint8_t kMaxPowerupLevel = 3;

enum class PowerupKind : std::uint8_t {
    Cannonball,
    GrapplingHook,
    Lightning,
    Missile,
    OilSlick,
    Dynamite,
    Magnet,
    Curse,
    Shield,
    Turbo,
    Warp,
    Count,
};

inline constexpr std::size_t kPowerupKindCount = static_cast<std::size_t>(PowerupKind::Count);

/// `entities` are claimed from the manager's shared pool, and each use starts `launches` effects.
struct PowerupKindSpec {
    std::size_t poolSize = 0;
    std::size_t entities = 0;
    std::size_t launches = 1;
};

/// `shot` numbers the launches of one use, such as each of the three missiles.
struct PowerupUse {
    std::uint8_t racer = 0;
    assets::BrickColour colour = assets::BrickColour::Red;
    std::uint8_t level = 0;
    std::uint8_t shot = 0;

    friend bool operator==(const PowerupUse&, const PowerupUse&) = default;
};

[[nodiscard]] PowerupKind KindFor(assets::BrickColour colour, std::uint8_t level);

[[nodiscard]] const PowerupKindSpec& SpecFor(PowerupKind kind);

}
