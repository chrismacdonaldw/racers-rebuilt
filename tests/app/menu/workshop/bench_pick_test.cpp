#include "app/menu/workshop/bench_pick.hpp"

#include <doctest/doctest.h>

#include <numbers>

#include "app/track_space.hpp"
#include "core/quaternion.hpp"
#include "sim/car_build/car_pieces.hpp"
#include "sim/car_build/loose_bricks.hpp"

namespace rr::app {
namespace {

constexpr render::Rect kView{.x = 0.0F, .y = 0.0F, .width = 100.0F, .height = 100.0F};
constexpr Vector3 kCarCorner{.x = 11.0F, .y = 13.0F, .z = 1.0F};

BenchView LookingDownFrom(const Vector3& game) {
    return {.camera = {.transform = {.position = ToEngine(game),
                                     .rotation = FromAxisAngle({.x = 1.0F},
                                                               -std::numbers::pi_v<float> / 2.0F)},
                       .verticalFovDegrees = 90.0F},
            .view = kView,
            .carCorner = kCarCorner,
            .groundCorner = {}};
}

struct Pieces {
    assets::PieceRecord chassis = sim::test::Chassis();
    assets::PieceRecord brick = sim::test::Block(0x900, 1, 1, 3);

    [[nodiscard]] sim::Bench Started() const {
        sim::Bench bench;
        REQUIRE(bench.car.Place(chassis, {}, 0, 0).has_value());
        REQUIRE(bench.ground.Put({.piece = &brick, .placement = {.x = 2, .y = 2}}).has_value());
        return bench;
    }
};

TEST_CASE("a pick over the car finds the car before the ground below it") {
    const Pieces pieces;
    const sim::Bench bench = pieces.Started();

    CHECK(PickBenchCell(LookingDownFrom({.x = 13.5F, .y = 14.5F, .z = 20.0F}), bench, 50.0F,
                        50.0F) == BenchCell{.area = sim::BenchArea::Car, .cell = {.x = 2, .y = 1}});
}

TEST_CASE("a pick away from the car finds the ground") {
    const Pieces pieces;
    const sim::Bench bench = pieces.Started();

    CHECK(PickBenchCell(LookingDownFrom({.x = 4.5F, .y = 5.5F, .z = 20.0F}), bench, 50.0F, 50.0F) ==
          BenchCell{.area = sim::BenchArea::Ground, .cell = {.x = 4, .y = 5}});
}

TEST_CASE("a piece pick finds the piece on top of the cell") {
    const Pieces pieces;
    const sim::Bench bench = pieces.Started();

    CHECK(PickBenchPiece(LookingDownFrom({.x = 2.5F, .y = 2.5F, .z = 20.0F}), bench, 50.0F,
                         50.0F) == BenchPiece{.area = sim::BenchArea::Ground, .index = 0});
    CHECK(PickBenchPiece(LookingDownFrom({.x = 13.5F, .y = 14.5F, .z = 20.0F}), bench, 50.0F,
                         50.0F) == BenchPiece{.area = sim::BenchArea::Car, .index = 0});
    CHECK_FALSE(
        PickBenchPiece(LookingDownFrom({.x = 4.5F, .y = 5.5F, .z = 20.0F}), bench, 50.0F, 50.0F)
            .has_value());
}

TEST_CASE("a placement stays on its own area's grid") {
    CHECK(PlaceOn({.area = sim::BenchArea::Ground, .cell = {.x = 31, .y = 31}},
                  {.width = 3, .height = 3}, 0) == sim::PiecePlacement{.x = 29, .y = 29});
    CHECK(PlaceOn({.area = sim::BenchArea::Car, .cell = {.x = 9, .y = 5}},
                  {.width = 3, .height = 3},
                  2) == sim::PiecePlacement{.x = 7, .y = 3, .rotation = 2});
}

TEST_CASE("a piece outside its area is missing") {
    const Pieces pieces;
    const sim::Bench bench = pieces.Started();

    CHECK(PieceOf(bench, {.area = sim::BenchArea::Ground, .index = 0})->piece == &pieces.brick);
    CHECK(PieceOf(bench, {.area = sim::BenchArea::Ground, .index = 1}) == nullptr);
}

}
}
