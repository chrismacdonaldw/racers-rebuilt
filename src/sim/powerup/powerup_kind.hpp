#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "assets/brick_layout.hpp"
#include "core/vector3.hpp"

namespace rr::sim {

inline constexpr std::uint8_t kMaxPowerupLevel = 3;
inline constexpr std::size_t kMaxLaunches = 3;

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
inline constexpr std::size_t kPowerupColourCount = 4;

/// Indexed by `assets::BrickColour`, then level.
using PowerupSlots = std::array<std::array<PowerupKind, kMaxPowerupLevel + 1>, kPowerupColourCount>;

inline constexpr PowerupSlots kOriginalSlots{{
    {PowerupKind::Cannonball, PowerupKind::GrapplingHook, PowerupKind::Lightning,
     PowerupKind::Missile},
    {PowerupKind::Shield, PowerupKind::Shield, PowerupKind::Shield, PowerupKind::Shield},
    {PowerupKind::Turbo, PowerupKind::Turbo, PowerupKind::Turbo, PowerupKind::Warp},
    {PowerupKind::OilSlick, PowerupKind::Dynamite, PowerupKind::Magnet, PowerupKind::Curse},
}};

struct PowerupKindSpec {
    std::size_t poolSize = 0;
    std::size_t entities = 0;
    std::size_t launches = 1;
};

/// In game space. `direction` is a unit vector.
struct HazardAim {
    Vector3 source;
    Vector3 target;
    Vector3 direction;

    friend bool operator==(const HazardAim&, const HazardAim&) = default;
};

/// A use with a `hazard` fires from that hazard's aim, and only a warp still has its `racer`.
struct PowerupUse {
    std::uint8_t racer = 0;
    assets::BrickColour colour = assets::BrickColour::Red;
    std::uint8_t level = 0;
    std::uint8_t shot = 0;
    std::optional<std::uint8_t> hazard;

    friend bool operator==(const PowerupUse&, const PowerupUse&) = default;
};

[[nodiscard]] PowerupKind KindFor(assets::BrickColour colour, std::uint8_t level,
                                  const PowerupSlots& slots = kOriginalSlots);

[[nodiscard]] const PowerupKindSpec& SpecFor(PowerupKind kind);

}
