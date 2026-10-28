#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

/// An absent value means the racer's default for it.
struct SurfaceEntry {
    std::string name;
    std::optional<std::int32_t> enterEvent;
    std::optional<std::int32_t> leaveEvent;
    std::optional<std::int32_t> touchEvent;
    std::optional<std::int32_t> projectileEvent;
    std::optional<Vector3> vector2C;
    std::optional<Vector3> force;
    std::optional<std::int32_t> soundId;
    std::optional<std::int32_t> value2F;
    std::optional<std::int32_t> value30;
    std::optional<std::string> particle;
    std::optional<float> supportThreshold;
    std::optional<float> friction;
    std::optional<float> lateralGrip;
    std::optional<float> value35;
    std::optional<float> rollingResistance;
    bool isNonSolid = false;
    bool isProjectilePassable = false;
    bool isFinish = false;
};

[[nodiscard]] Result<std::vector<SurfaceEntry>> ParseSurfaceTable(std::span<const std::byte> bytes);

}
