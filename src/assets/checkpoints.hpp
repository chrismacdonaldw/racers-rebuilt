#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

inline constexpr std::uint8_t kNoLink = 255;

/// `normal · p + distance = 0` on the gate, the normal faces backward, and `links[0]` is next.
struct Checkpoint {
    Vector3 normal;
    float distance = 0.0F;
    Vector3 position;
    std::array<std::uint8_t, 4> links{};
};

struct CheckpointsAsset {
    std::vector<Checkpoint> checkpoints;
};

[[nodiscard]] Result<CheckpointsAsset> ParseCpb(std::span<const std::byte> bytes);

}
