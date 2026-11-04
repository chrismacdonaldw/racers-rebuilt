#include "assets/minifig_skin.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace rr::assets {
namespace {

constexpr std::uint16_t kUpperJoint = 1;
constexpr std::uint16_t kLowerJoint = 2;
constexpr float kStretchedShare = 0.01F;

std::uint16_t Limb(const Vector3& point) {
    return point.x < 1.0F ? kUpperJoint : kLowerJoint;
}

std::array<SkinCorner, 3> Flat(const Vector3& a, const Vector3& b, const Vector3& c) {
    constexpr Vector3 kUp{.z = 1.0F};
    return {SkinCorner{.position = a, .normal = kUp}, SkinCorner{.position = b, .normal = kUp},
            SkinCorner{.position = c, .normal = kUp}};
}

float Area(const std::array<SkinCorner, 3>& corners) {
    return Length(Cross(corners[1].position - corners[0].position,
                        corners[2].position - corners[0].position)) *
           0.5F;
}

bool FollowsOneJoint(const SkinnedTriangle& triangle) {
    return triangle.joints[0] == triangle.joints[1] && triangle.joints[1] == triangle.joints[2];
}

bool Follows(const std::vector<SkinnedTriangle>& triangles, std::uint16_t joint) {
    return std::ranges::any_of(
        triangles, [&](const SkinnedTriangle& triangle) { return triangle.joints[0] == joint; });
}

TEST_CASE("a triangle on one joint stays whole") {
    const std::array<SkinCorner, 3> corners =
        Flat({.x = 0.1F, .y = 0.1F}, {.x = 0.4F, .y = 0.1F}, {.x = 0.1F, .y = 0.4F});

    const std::vector<SkinnedTriangle> skinned = SkinTriangle(Limb, corners);

    REQUIRE(skinned.size() == 1);
    CHECK(skinned[0].joints == std::array{kUpperJoint, kUpperJoint, kUpperJoint});
}

TEST_CASE("a triangle across two joints is cut where the joint changes") {
    const std::array<SkinCorner, 3> corners =
        Flat({.x = 0.5F, .y = 0.1F}, {.x = 1.5F, .y = 0.1F}, {.x = 1.0F, .y = 0.3F});

    const std::vector<SkinnedTriangle> skinned = SkinTriangle(Limb, corners);

    float area = 0.0F;
    float stretched = 0.0F;
    for (const SkinnedTriangle& triangle : skinned) {
        area += Area(triangle.corners);
        stretched += FollowsOneJoint(triangle) ? 0.0F : Area(triangle.corners);
    }
    CHECK(Follows(skinned, kUpperJoint));
    CHECK(Follows(skinned, kLowerJoint));
    CHECK(area == doctest::Approx(Area(corners)));
    CHECK(stretched < area * kStretchedShare);
}

}
}
