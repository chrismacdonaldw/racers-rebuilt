#include "app/decal_clip.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace rr::app {
namespace {

constexpr float kTolerance = 1e-5F;

DecalVertex At(float u, float v) {
    return {.position = {.x = u, .y = v}, .u = u, .v = v};
}

std::span<const DecalVertex> CornersOf(const DecalPolygon& polygon) {
    return std::span(polygon.corners).first(polygon.count);
}

bool IsNear(float actual, float expected) {
    return std::abs(actual - expected) <= kTolerance;
}

bool IsOnFarU(const DecalVertex& vertex) {
    return IsNear(vertex.position.x, 1.0F);
}

bool IsOnFarV(const DecalVertex& vertex) {
    return IsNear(vertex.position.y, 1.0F);
}

bool StoresFarEdges(const DecalVertex& vertex) {
    const bool storesU = !IsOnFarU(vertex) || IsNear(vertex.u, 0.99F);
    const bool storesV = !IsOnFarV(vertex) || IsNear(vertex.v, 0.99F);
    return storesU && storesV && vertex.u <= 0.99F && vertex.v <= 0.99F;
}

bool SitsOnItsCoordinates(const DecalVertex& vertex) {
    return vertex.u >= 0.0F && vertex.v >= 0.0F && IsNear(vertex.position.x, vertex.u) &&
           IsNear(vertex.position.y, vertex.v);
}

TEST_CASE("a triangle inside the decal stays whole") {
    const DecalPolygon clipped = ClipToDecal({At(0.2F, 0.2F), At(0.8F, 0.2F), At(0.2F, 0.8F)});

    REQUIRE(clipped.count == 3);
    CHECK(clipped.corners[1].u == doctest::Approx(0.8F));
}

TEST_CASE("corners cut at the far edges lie on them but take 0.99") {
    const DecalPolygon clipped = ClipToDecal({At(0.5F, 0.2F), At(1.5F, 0.2F), At(0.5F, 1.4F)});

    REQUIRE(clipped.count == 5);
    CHECK(std::ranges::count_if(CornersOf(clipped), IsOnFarU) == 2);
    CHECK(std::ranges::count_if(CornersOf(clipped), IsOnFarV) == 2);
    CHECK(std::ranges::all_of(CornersOf(clipped), StoresFarEdges));
}

TEST_CASE("corners cut at the near edges sit at 0") {
    const DecalPolygon clipped = ClipToDecal({At(-0.5F, 0.5F), At(0.5F, 0.5F), At(0.5F, -0.5F)});

    REQUIRE(clipped.count >= 4);
    CHECK(std::ranges::all_of(CornersOf(clipped), SitsOnItsCoordinates));
}

}
}
