#include "render/joint_uniforms.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "../assets/archive_survey.hpp"
#include "assets/jam_archive.hpp"
#include "assets/skeleton_database.hpp"

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

TEST_CASE("every retail skeleton fits the joint uniforms") {
    const std::optional<assets::JamArchive> jam = test::OpenRetailJam();
    if (!jam) {
        return;
    }
    std::size_t most = 0;
    const test::Survey survey = test::SurveyLayer(
        *jam, [](std::string_view name) { return test::Extension(name) == "SDB"; },
        [&](std::span<const std::byte> bytes) {
            return assets::ParseSkeleton(bytes).transform(
                [&](const assets::SkeletonAsset& s) { most = std::max(most, s.nodes.size()); });
        });

    MESSAGE("the largest retail skeleton has ", most, " nodes");
    CHECK(survey.failures.empty());
    CHECK(most <= kMaxJoints);
}

}
}
