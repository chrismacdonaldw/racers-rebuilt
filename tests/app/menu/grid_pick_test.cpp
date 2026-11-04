#include "app/menu/grid_pick.hpp"

#include <doctest/doctest.h>

#include <numbers>

#include "app/track_space.hpp"
#include "core/quaternion.hpp"
#include "sim/car_build/car_pieces.hpp"

namespace rr::app {
namespace {

constexpr render::Rect kView{.x = 0.0F, .y = 0.0F, .width = 100.0F, .height = 100.0F};

GridView LookingDownFrom(const Vector3& game) {
    return {.camera = {.transform = {.position = ToEngine(game),
                                     .rotation = FromAxisAngle({.x = 1.0F},
                                                               -std::numbers::pi_v<float> / 2.0F)},
                       .verticalFovDegrees = 90.0F},
            .view = kView};
}

TEST_CASE("the view centre picks the cell straight below the camera") {
    const sim::CarGrid grid;
    const GridView view = LookingDownFrom({.x = 2.5F, .y = 3.5F, .z = 10.0F});

    CHECK(PickGridCell(view, grid, 50.0F, 50.0F) == GridCellPick{.x = 2, .y = 3});
    CHECK(PickGridCell(view, grid, 75.0F, 50.0F) == GridCellPick{.x = 7, .y = 3});
    CHECK_FALSE(PickGridCell(view, grid, 99.0F, 50.0F).has_value());
}

TEST_CASE("a tall column in front of a cell takes the pick") {
    sim::CarGrid grid;
    grid.Stamp(sim::test::Block(1, 1, 1, 10), {.x = 5, .y = 2}, 0, 0);
    const GridView view = LookingDownFrom({.x = 2.5F, .y = 2.5F, .z = 10.0F});

    CHECK(PickGridCell(view, grid, 75.0F, 50.0F) == GridCellPick{.x = 5, .y = 2});
}

TEST_CASE("a camera that looks level picks nothing") {
    const sim::CarGrid grid;
    GridView view = LookingDownFrom({.x = 2.5F, .y = -5.0F, .z = 1.0F});
    view.camera.transform.rotation = {};

    CHECK_FALSE(PickGridCell(view, grid, 50.0F, 50.0F).has_value());
}

TEST_CASE("a piece centres on the picked cell without leaving the grid") {
    CHECK(CentredOn({.x = 4, .y = 3}, {.width = 3, .height = 2}, 1) ==
          sim::PiecePlacement{.x = 3, .y = 3, .rotation = 1});
    CHECK(CentredOn({.x = 9, .y = 0}, {.width = 4, .height = 4}, 0) ==
          sim::PiecePlacement{.x = 6, .y = 0, .rotation = 0});
}

}
}
