#include "app/decal_surface.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <limits>
#include <vector>

namespace rr::app {
namespace {

TEST_CASE("gathering names each triangle under an area once") {
    const DecalSurface surface(std::vector<DecalTriangle>{
        {Vector3{.x = 0.0F, .z = 0.0F}, Vector3{.x = 100.0F, .z = 0.0F},
         Vector3{.x = 0.0F, .z = 100.0F}},
        {Vector3{.x = 500.0F, .z = 500.0F}, Vector3{.x = 501.0F, .z = 500.0F},
         Vector3{.x = 500.0F, .z = 501.0F}}});
    std::vector<std::uint32_t> found{9};

    surface.Gather({.minX = 10.0F, .minZ = 10.0F, .maxX = 60.0F, .maxZ = 60.0F}, found);
    CHECK(found == std::vector<std::uint32_t>{0});

    surface.Gather({.minX = 495.0F, .minZ = 495.0F, .maxX = 505.0F, .maxZ = 505.0F}, found);
    CHECK(found == std::vector<std::uint32_t>{1});

    surface.Gather({.minX = -50.0F, .minZ = -50.0F, .maxX = 600.0F, .maxZ = 600.0F}, found);
    CHECK(found == std::vector<std::uint32_t>{0, 1});

    surface.Gather({.minX = -50.0F, .minZ = -50.0F, .maxX = -40.0F, .maxZ = -40.0F}, found);
    CHECK(found.empty());
}

TEST_CASE("an empty surface gathers nothing") {
    const DecalSurface surface;
    std::vector<std::uint32_t> found{3};

    surface.Gather({.maxX = 10.0F, .maxZ = 10.0F}, found);

    CHECK(found.empty());
}

TEST_CASE("triangles with non-finite corners are left out of the surface") {
    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    const DecalSurface surface(std::vector<DecalTriangle>{
        {Vector3{.x = 0.0F, .z = 0.0F}, Vector3{.x = 100.0F, .z = 0.0F},
         Vector3{.x = 0.0F, .z = 100.0F}},
        {Vector3{.x = std::numeric_limits<float>::quiet_NaN()}, Vector3{.x = 1.0F},
         Vector3{.z = 1.0F}},
        {Vector3{.x = kInfinity}, Vector3{.x = 1.0F}, Vector3{.z = 1.0F}}});
    std::vector<std::uint32_t> found;

    surface.Gather({.minX = -kInfinity, .minZ = -kInfinity, .maxX = kInfinity, .maxZ = kInfinity},
                   found);

    REQUIRE(surface.Triangles().size() == 1);
    CHECK(found == std::vector<std::uint32_t>{0});
}

TEST_CASE("a surface too wide to measure takes no triangles") {
    constexpr float kFar = std::numeric_limits<float>::max();
    const DecalSurface surface(
        std::vector<DecalTriangle>{{Vector3{.x = -kFar}, Vector3{.x = kFar}, Vector3{.z = 1.0F}}});

    CHECK(surface.Triangles().empty());
}

}
}
