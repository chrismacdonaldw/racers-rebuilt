#pragma once

#include "core/transform.hpp"

#include <glm/mat4x4.hpp>

namespace rr::render {

/// Looks down its local -Z axis with +Y up.
struct Camera {
    Transform transform;
    float verticalFovDegrees = 60.0F;
    float nearPlane = 0.1F;
    float farPlane = 1000.0F;
};

[[nodiscard]] glm::mat4 ModelMatrix(const Transform& transform);

/// Maps to D3D clip space, with depth from 0 at the near plane to 1 at the far plane.
[[nodiscard]] glm::mat4 ViewProjection(const Camera& camera, float aspectRatio);

}
