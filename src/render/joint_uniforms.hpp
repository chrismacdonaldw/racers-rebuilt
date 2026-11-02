#pragma once

#include <array>
#include <span>

#include "core/transform.hpp"
#include "render/render_view.hpp"

#include <glm/mat4x4.hpp>

namespace rr::render {

/// Laid out as `JointUniforms` in `skinned_mesh.vert.hlsl`.
struct JointUniforms {
    std::array<glm::mat4, kMaxJoints> joints;
};

/// Joints past `kMaxJoints` are dropped, and missing ones are the identity.
[[nodiscard]] JointUniforms ToJointUniforms(std::span<const Transform> joints);

}
