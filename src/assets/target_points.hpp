#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

struct TargetPoint {
    Vector3 position;
    std::optional<std::int32_t> removingEvent;

    friend bool operator==(const TargetPoint&, const TargetPoint&) = default;
};

[[nodiscard]] Result<std::vector<TargetPoint>> ParseTgb(std::span<const std::byte> bytes);

}
