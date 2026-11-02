#include "render/joint_uniforms.hpp"

#include <doctest/doctest.h>

#include <vector>

namespace rr::render {
namespace {

TEST_CASE("joints become model matrices, and missing joints stay put") {
    const std::vector<Transform> joints{{.position = {.x = 1.0F, .y = 2.0F, .z = 3.0F}}};

    const JointUniforms uniforms = ToJointUniforms(joints);

    CHECK(uniforms.joints[0][3] == glm::vec4(1.0F, 2.0F, 3.0F, 1.0F));
    CHECK(uniforms.joints[1] == glm::mat4(1.0F));
    CHECK(uniforms.joints[kMaxJoints - 1] == glm::mat4(1.0F));
}

TEST_CASE("joints past the uniform array are dropped") {
    const std::vector<Transform> joints(kMaxJoints + 5, Transform{.position = {.z = 4.0F}});

    const JointUniforms uniforms = ToJointUniforms(joints);

    CHECK(uniforms.joints[kMaxJoints - 1][3].z == 4.0F);
}

}
}
