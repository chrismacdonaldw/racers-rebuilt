#include "render/joint_uniforms.hpp"

#include <algorithm>

#include "render/camera.hpp"

namespace rr::render {

JointUniforms ToJointUniforms(std::span<const Transform> joints) {
    JointUniforms uniforms;
    uniforms.joints.fill(glm::mat4(1.0F));
    const std::size_t count = std::min(joints.size(), kMaxJoints);
    for (std::size_t i = 0; i < count; ++i) {
        uniforms.joints.at(i) = ModelMatrix(joints[i]);
    }
    return uniforms;
}

}
