#include "assets/surface_transfer.hpp"

#include <doctest/doctest.h>

#include <array>
#include <vector>

namespace rr::assets {
namespace {

SurfaceTriangle Flat(float z, float u, bool isFacingUp) {
    const Vector3 a{.x = 0.0F, .y = 0.0F, .z = z};
    const Vector3 b{.x = 1.0F, .y = 0.0F, .z = z};
    const Vector3 c{.x = 0.0F, .y = 1.0F, .z = z};
    SurfaceTriangle triangle =
        isFacingUp ? MakeSurfaceTriangle({a, b, c}) : MakeSurfaceTriangle({a, c, b});
    triangle.u = {u, u + 1.0F, u};
    triangle.v = {0.0F, 0.0F, 1.0F};
    return triangle;
}

constexpr Vector3 kUp{.z = 1.0F};

TEST_CASE("a point takes the UV under it on the nearest triangle") {
    const std::vector surface{Flat(0.0F, 0.0F, true), Flat(5.0F, 10.0F, true)};

    const SurfacePoint point =
        NearestSurfacePoint(surface, {.x = 0.5F, .y = 0.25F, .z = 0.1F}, kUp);

    REQUIRE(point.triangle == surface.data());
    CHECK(point.u == doctest::Approx(0.5F));
    CHECK(point.v == doctest::Approx(0.25F));
    CHECK(point.distanceSquared == doctest::Approx(0.01F));
}

TEST_CASE("a point past a triangle's edge takes the UV of the closest edge point") {
    const std::vector surface{Flat(0.0F, 0.0F, true)};

    const SurfacePoint point =
        NearestSurfacePoint(surface, {.x = 2.0F, .y = -1.0F, .z = 0.0F}, kUp);

    CHECK(point.u == doctest::Approx(1.0F));
    CHECK(point.v == doctest::Approx(0.0F));
}

TEST_CASE("a slightly nearer triangle facing away loses to one facing the same way") {
    const std::vector surface{Flat(0.0F, 0.0F, false), Flat(0.15F, 10.0F, true)};

    const SurfacePoint point =
        NearestSurfacePoint(surface, {.x = 0.25F, .y = 0.25F, .z = 0.05F}, kUp);

    CHECK(point.triangle == &surface[1]);
    CHECK(point.u == doctest::Approx(10.25F));
}

SurfaceTriangle Strip(float left, float u) {
    SurfaceTriangle triangle = MakeSurfaceTriangle(
        {Vector3{.x = left}, Vector3{.x = left + 1.0F}, Vector3{.x = left, .y = 1.0F}});
    triangle.u = {u, u + 0.4F, u};
    return triangle;
}

TEST_CASE("a triangle across a texture seam keeps the UVs of the side under its centre") {
    const std::vector surface{Strip(0.0F, 0.5F), Strip(1.0F, 0.0F)};

    const std::array<SurfacePoint, 3> points =
        NearestSurfacePoints(surface,
                             {Vector3{.x = 0.6F, .y = 0.1F}, Vector3{.x = 0.8F, .y = 0.1F},
                              Vector3{.x = 1.1F, .y = 0.1F}},
                             kUp);

    CHECK(points[0].u == doctest::Approx(0.74F));
    CHECK(points[2].triangle == surface.data());
    CHECK(points[2].u == doctest::Approx(0.94F));
}

TEST_CASE("a triangle on a smooth surface takes each corner's own UV") {
    const std::vector surface{Strip(0.0F, 0.0F), Strip(1.0F, 0.4F)};

    const std::array<SurfacePoint, 3> points =
        NearestSurfacePoints(surface,
                             {Vector3{.x = 0.6F, .y = 0.1F}, Vector3{.x = 0.8F, .y = 0.1F},
                              Vector3{.x = 1.1F, .y = 0.1F}},
                             kUp);

    CHECK(points[2].triangle == &surface[1]);
    CHECK(points[2].u == doctest::Approx(0.44F));
}

TEST_CASE("no surface gives no triangle") {
    CHECK(NearestSurfacePoint({}, {}, kUp).triangle == nullptr);
}

}
}
