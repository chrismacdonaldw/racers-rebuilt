#pragma once

#include <cstddef>
#include <memory>
#include <span>

#include "core/result.hpp"
#include "core/vector3.hpp"

#include <cgltf.h>
#include <glm/vec3.hpp>

namespace rr::assets {

using GltfData = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>;

/// Every buffer must be embedded in the file, which is then validated.
[[nodiscard]] Result<GltfData> ParseGlb(std::span<const std::byte> bytes);

/// glTF is Y-up and the game Z-up, both right-handed.
[[nodiscard]] inline Vector3 ToGame(const glm::vec3& gltf) {
    return {.x = gltf.x, .y = -gltf.z, .z = gltf.y};
}

}
