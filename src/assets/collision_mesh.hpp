#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

inline constexpr std::int32_t kNoChild = -1;

struct CollisionTriangle {
    std::array<std::uint16_t, 3> vertices{};
    std::uint16_t material = 0;
};

/// The plane passes through vertex 0 of the first triangle. `front` holds positive distances.
struct CollisionNode {
    std::int32_t front = kNoChild;
    std::int32_t back = kNoChild;
    Vector3 normal;
    std::uint16_t firstTriangle = 0;
    std::uint16_t triangleCount = 0;
};

struct CollisionMeshAsset {
    std::vector<std::string> materials;
    std::vector<Vector3> vertices;
    std::vector<CollisionTriangle> triangles;
    std::vector<CollisionNode> nodes;
};

/// Fails on any index past its list. Both stored "no child" values become `kNoChild`. The tree
/// shape is not checked.
[[nodiscard]] Result<CollisionMeshAsset> ParseCollisionMesh(std::span<const std::byte> bytes);

}
