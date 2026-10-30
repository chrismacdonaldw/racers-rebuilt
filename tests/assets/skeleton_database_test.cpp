#include "assets/skeleton_database.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Floats;
using test::Join;
using test::NamedEntry;
using test::NamedLibrary;
using test::Text;

TEST_CASE("skeleton nodes read their placement and resolve parents") {
    const std::vector sdb = NamedLibrary(
        0x27, {
                  NamedEntry(0x27, "alpelvis", {}),
                  NamedEntry(0x27, "torso",
                             Join({Bytes({0x28}), Floats({0, 0, 45}), Bytes({0x29}),
                                   Floats({0, 0, 1, 1}), Bytes({0x2A}), Text("alpelvis")})),
                  NamedEntry(0x27, "head", Join({Bytes({0x2A}), Text("torso")})),
              });

    const Result<SkeletonAsset> skeleton = ParseSkeleton(sdb);

    REQUIRE(skeleton.has_value());
    REQUIRE(skeleton->nodes.size() == 3);
    const SkeletonNode& root = skeleton->nodes[0];
    CHECK(root.name == "alpelvis");
    CHECK_FALSE(root.parent.has_value());
    CHECK(root.position == Vector3{});
    CHECK(root.rotation == Quaternion{});

    const SkeletonNode& torso = skeleton->nodes[1];
    CHECK(torso.parent == 0);
    CHECK(torso.position == Vector3{.z = 45.0F});
    const float half = std::sqrt(0.5F);
    CHECK(torso.rotation.w == doctest::Approx(half));
    CHECK(torso.rotation.z == doctest::Approx(-half));
    CHECK(skeleton->nodes[2].parent == 1);
}

TEST_CASE("a skeleton node whose parent comes later fails") {
    const std::vector sdb =
        NamedLibrary(0x27, {NamedEntry(0x27, "torso", Join({Bytes({0x2A}), Text("alpelvis")})),
                            NamedEntry(0x27, "alpelvis", {})});

    CHECK_FALSE(ParseSkeleton(sdb).has_value());
}

TEST_CASE("a skeleton with no nodes fails") {
    CHECK_FALSE(ParseSkeleton(NamedLibrary(0x27, {})).has_value());
}

TEST_CASE("a skeleton node with a zero rotation fails") {
    const std::vector sdb =
        NamedLibrary(0x27, {NamedEntry(0x27, "root", Join({Bytes({0x29}), Floats({0, 0, 0, 0})}))});

    CHECK_FALSE(ParseSkeleton(sdb).has_value());
}

}
}
