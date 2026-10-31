#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

enum class BrickColour : std::uint8_t {
    Red,
    Blue,
    Green,
    Yellow,
};

inline constexpr Milliseconds kDefaultBrickRespawn{3000};

struct ColourBrickPlacement {
    Vector3 position;
    BrickColour colour = BrickColour::Green;
    Milliseconds respawn = kDefaultBrickRespawn;
};

struct BrickLayout {
    std::vector<ColourBrickPlacement> coloured;
    std::vector<Vector3> white;
};

[[nodiscard]] Result<BrickLayout> ParsePwb(std::span<const std::byte> bytes);

}
