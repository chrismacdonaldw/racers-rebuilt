#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

struct StartSlot {
    Vector3 position;
    Vector3 forward;
    Vector3 up;
};

/// Slots in slot order. Racer `i` starts in slot `i`.
struct StartGridAsset {
    std::vector<StartSlot> slots;
};

[[nodiscard]] Result<StartGridAsset> ParseSpb(std::span<const std::byte> bytes);

}
