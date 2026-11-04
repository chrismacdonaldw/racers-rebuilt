#include "assets/face_shapes.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rr::assets {
namespace {

bool IsInside(const std::vector<std::uint8_t>& mask, std::size_t x, std::size_t y) {
    return mask.at((y * kFaceDrawingSizePx) + x) != 0;
}

std::size_t CountInside(const std::vector<std::uint8_t>& mask) {
    return static_cast<std::size_t>(std::ranges::count(mask, std::uint8_t{1}));
}

FaceShape Rectangle(double left, double top, double right, double bottom) {
    return FacePolygon{.points = {{left, top}, {right, top}, {right, bottom}, {left, bottom}}};
}

TEST_CASE("a pixel is inside a shape when at least half its samples are") {
    CHECK(CountInside(ShapeMask(Rectangle(10.0, 10.0, 20.0, 20.0))) == 100);
    CHECK(IsInside(ShapeMask(Rectangle(10.0, 10.0, 10.5, 11.0)), 10, 10));
    CHECK_FALSE(IsInside(ShapeMask(Rectangle(10.0, 10.0, 10.3, 11.0)), 10, 10));
}

TEST_CASE("a polygon fills by nonzero winding") {
    const FaceShape twice = FacePolygon{.points = {{10.0, 10.0},
                                                   {20.0, 10.0},
                                                   {20.0, 20.0},
                                                   {10.0, 20.0},
                                                   {10.0, 10.0},
                                                   {20.0, 10.0},
                                                   {20.0, 20.0},
                                                   {10.0, 20.0}}};

    CHECK(CountInside(ShapeMask(twice)) == 100);
}

TEST_CASE("a line has round caps of half its width") {
    const std::vector<std::uint8_t> mask =
        ShapeMask(FaceLine{.width = 8.0, .points = {{50.0, 50.0}, {100.0, 50.0}}});

    CHECK(IsInside(mask, 75, 53));
    CHECK_FALSE(IsInside(mask, 75, 55));
    CHECK(IsInside(mask, 47, 50));
    CHECK_FALSE(IsInside(mask, 44, 50));
    CHECK_FALSE(IsInside(mask, 46, 46));
}

TEST_CASE("a single point line is a dot") {
    CHECK(CountInside(ShapeMask(FaceLine{.width = 4.0, .points = {{30.0, 30.0}}})) > 0);
}

TEST_CASE("an ellipse turns clockwise by its angle") {
    const FaceEllipse flat{.centre = {128.0, 128.0}, .radii = {40.0, 10.0}};
    FaceEllipse upright = flat;
    upright.degrees = 90.0;

    CHECK(IsInside(ShapeMask(flat), 160, 128));
    CHECK_FALSE(IsInside(ShapeMask(flat), 128, 160));
    CHECK(IsInside(ShapeMask(upright), 128, 160));
    CHECK_FALSE(IsInside(ShapeMask(upright), 160, 128));
}

TEST_CASE("everywhere covers the whole face") {
    CHECK(CountInside(ShapeMask(FaceEverywhere{})) ==
          std::size_t{kFaceDrawingSizePx} * kFaceDrawingSizePx);
}

TEST_CASE("shapes past the edges are clipped") {
    CHECK(CountInside(ShapeMask(Rectangle(-50.0, -50.0, 2.0, 2.0))) == 4);
    CHECK(CountInside(ShapeMask(Rectangle(300.0, 300.0, 400.0, 400.0))) == 0);
}

}
}
