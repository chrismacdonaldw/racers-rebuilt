#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "assets/brick_layout.hpp"

namespace rr::app {

inline constexpr std::array kBrickColours{assets::BrickColour::Red, assets::BrickColour::Yellow,
                                          assets::BrickColour::Blue, assets::BrickColour::Green};

[[nodiscard]] std::string_view ColourName(std::optional<assets::BrickColour> colour);

/// Such as `Homing missiles (red 3)`.
[[nodiscard]] std::string PowerupLabel(assets::BrickColour colour, std::uint8_t level);

}
