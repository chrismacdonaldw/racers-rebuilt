#include "sim/animation/skeleton_pose.hpp"

#include <doctest/doctest.h>

#include <numbers>
#include <span>

namespace rr::sim {
namespace {

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
}

assets::SkeletonAsset Arm() {
    const Quaternion quarterTurn = FromAxisAngle({.z = 1.0F}, std::numbers::pi_v<float> / 2.0F);
    return {.nodes = {{.name = "root", .position = {.x = 64.0F}, .rotation = quarterTurn},
                      {.name = "arm", .position = {.x = 128.0F}, .parent = 0},
                      {.name = "hand", .position = {.z = 64.0F}, .parent = 1}}};
}

TEST_CASE("a node is placed in its parent's space and scaled into model units") {
    SkeletonPose pose(Arm(), 1.0F / 64.0F);

    const std::span<const Transform> nodes = pose.Solve();

    REQUIRE(nodes.size() == 3);
    CheckNear(nodes[0].position, {.x = 1.0F});
    CheckNear(nodes[1].position, {.x = 1.0F, .y = 2.0F});
    CheckNear(nodes[2].position, {.x = 1.0F, .y = 2.0F, .z = 1.0F});
    CheckNear(Apply(nodes[2], {.x = 1.0F}), {.x = 1.0F, .y = 3.0F, .z = 1.0F});
}

TEST_CASE("changed local placements move the nodes below them") {
    SkeletonPose pose(Arm(), 1.0F);

    pose.Locals()[1].position = {.x = 10.0F};
    const std::span<const Transform> nodes = pose.Solve();

    CheckNear(nodes[2].position, {.x = 64.0F, .y = 10.0F, .z = 64.0F});
}

}
}
