#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "assets/brick_layout.hpp"
#include "assets/driver_database.hpp"

namespace rr::sim {

inline constexpr std::size_t kBrickColourCount = 4;
inline constexpr std::uint8_t kSureUseChance = 255;

struct PowerupHabits {
    std::array<std::uint8_t, kBrickColourCount> useChances{kSureUseChance, kSureUseChance,
                                                           kSureUseChance, kSureUseChance};
    std::optional<assets::BrickColour> chargeColour;
    std::size_t chargeWhites = 0;

    [[nodiscard]] std::uint8_t UseChance(assets::BrickColour colour) const {
        return useChances[static_cast<std::size_t>(colour)];
    }
};

/// Out of 255, from a driver stat of 0 to 100.
[[nodiscard]] std::uint8_t UseChance(std::int32_t stat);

[[nodiscard]] PowerupHabits HabitsOf(const assets::DriverEntry& driver);

}
