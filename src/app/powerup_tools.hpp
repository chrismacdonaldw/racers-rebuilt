#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "assets/brick_layout.hpp"

namespace rr::app {

inline constexpr std::array kBrickColours{assets::BrickColour::Red, assets::BrickColour::Yellow,
                                          assets::BrickColour::Blue, assets::BrickColour::Green};

/// Such as `Homing missiles (red 3)`.
[[nodiscard]] std::string PowerupLabel(assets::BrickColour colour, std::uint8_t level);

}
