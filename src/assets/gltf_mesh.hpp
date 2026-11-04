#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "assets/geometry_asset.hpp"
#include "assets/glb_writer.hpp"
#include "assets/skeleton_database.hpp"
#include "core/quaternion.hpp"
#include "core/vector3.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace rr::assets {

[[nodiscard]] glm::vec3 ToGltf(const Vector3& game);

[[nodiscard]] glm::quat ToGltf(const Quaternion& game);

/// Each node's model-to-skeleton matrix in glTF space.
[[nodiscard]] std::vector<glm::mat4> BindPose(const SkeletonAsset& skeleton, float scale);

/// Poses vertices by `binds` when given.
struct MeshPose {
    std::span<const glm::mat4> binds;
    bool isSkinned = false;
};

/// `materials` maps each of `geometry`'s materials to the file's.
[[nodiscard]] std::string WriteMesh(GlbWriter& writer, const GeometryAsset& geometry,
                                    const MeshPose& pose, std::span<const std::size_t> materials);

}
