#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

/// `emitChance` is the share of spawn attempts skipped, and `points` are spawn velocities.
struct EmitterAsset {
    std::string name;
    float emitRate = 0.0F;
    float emitChance = 0.0F;
    Vector3 acceleration;
    std::vector<Vector3> points;
    float startSizeUp = 1.0F;
    float startSizeForward = 1.0F;
    float sizeGrowthUp = 0.0F;
    float sizeGrowthForward = 0.0F;
    std::optional<std::size_t> materialTrack;
    std::optional<std::string> material;
    Milliseconds particleLife{0};
    std::optional<Milliseconds> duration;
    float radius = 0.0F;
    std::uint8_t priority = 0;
};

[[nodiscard]] Result<std::vector<EmitterAsset>> ParseEmitterLibrary(
    std::span<const std::byte> bytes);

}
