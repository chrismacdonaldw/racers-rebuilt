#include "sim/car_build/built_car.hpp"

#include <doctest/doctest.h>

#include <expected>
#include <optional>

#include "sim/car_build/car_pieces.hpp"

namespace rr::sim {
namespace {

using test::Block;
using test::Cell;
using test::Piece;

struct Pieces {
    assets::PieceRecord chassis = test::Chassis();
    assets::PieceRecord brick = Block(0x900, 1, 1, 3);
    assets::PieceRecord wide = Block(0x901, 2, 1, 3);
    assets::PieceRecord plate = Block(0x902, 1, 1, 1);

    [[nodiscard]] BuiltCar WithChassis() const {
        BuiltCar build;
        REQUIRE(build.Place(chassis, {}, 4, 3) == 0);
        return build;
    }
};

std::expected<int, PlacementFailure> Fails(PlacementFailure failure) {
    return std::unexpected(failure);
}

int PlaceAcross(BuiltCar& build, const assets::PieceRecord& piece, int count) {
    int placed = 0;
    for (int cell = 0; cell < count; ++cell) {
        const PiecePlacement at{.x = (cell / kCarGridDepth) % kCarGridWidth,
                                .y = cell % kCarGridDepth};
        placed += build.Place(piece, at, 0, 0).has_value() ? 1 : 0;
    }
    return placed;
}

TEST_CASE_FIXTURE(Pieces, "the chassis rests on the bare grid and bricks rest on it") {
    BuiltCar build = WithChassis();

    CHECK(build.Place(brick, {.x = 2, .y = 1}, 7, 0) == 1);
    CHECK(build.Place(brick, {.x = 2, .y = 1}, 7, 0) == 4);
    CHECK(build.Grid().At(2, 1).height == 7);
    CHECK(build.Grid().At(2, 1).piece == 2);
    CHECK(build.Grid().At(3, 1).height == 1);
}

TEST_CASE_FIXTURE(Pieces, "a piece must sit on the grid and on something") {
    const BuiltCar bare;
    const BuiltCar build = WithChassis();

    CHECK(bare.Test(brick, {}) == Fails(PlacementFailure::NoSupport));
    CHECK(build.Test(brick, {.x = -1}) == Fails(PlacementFailure::OffLeftOrTop));
    CHECK(build.Test(brick, {.y = -1}) == Fails(PlacementFailure::OffLeftOrTop));
    CHECK(build.Test(wide, {.x = 9}) == Fails(PlacementFailure::PastRight));
    CHECK(build.Test(wide, {.x = 9, .y = 5, .rotation = 1}) == Fails(PlacementFailure::PastBottom));
}

TEST_CASE_FIXTURE(Pieces, "an overhanging piece rests on its highest support") {
    BuiltCar build = WithChassis();
    REQUIRE(build.Place(brick, {}, 0, 0) == 1);

    CHECK(build.Place(wide, {}, 0, 0) == 4);
    CHECK(build.Grid().At(1, 0).height == 7);
}

TEST_CASE_FIXTURE(Pieces, "a cell that doesn't rest can't sink into or sit level on what's below") {
    const assets::PieceRecord hook = Piece(0x903, 2, 1, {Cell(3), Cell(3, 0)});
    const BuiltCar level = WithChassis();
    BuiltCar sinking = WithChassis();
    REQUIRE(sinking.Place(brick, {.x = 1}, 0, 0) == 1);

    CHECK(level.Test(hook, {}) == Fails(PlacementFailure::Collision));
    CHECK(sinking.Test(hook, {}) == Fails(PlacementFailure::Collision));
    CHECK(level.Test(Piece(0x904, 2, 1, {Cell(3), Cell(3, 0x40)}), {}) == 1);
}

TEST_CASE_FIXTURE(Pieces, "a car stops at 15 plates") {
    BuiltCar build = WithChassis();
    for (int stacked = 0; stacked < 4; ++stacked) {
        REQUIRE(build.Place(brick, {}, 0, 0).has_value());
    }

    CHECK(build.Test(brick, {}) == Fails(PlacementFailure::TooHigh));
    CHECK(build.Test(plate, {}) == 13);
}

TEST_CASE_FIXTURE(Pieces, "a car holds 64 pieces with its chassis") {
    BuiltCar build = WithChassis();

    CHECK(PlaceAcross(build, plate, 63) == 63);
    CHECK(build.Pieces().size() == kMaxCarPieces);
    CHECK(build.Test(plate, {.x = 5}) == Fails(PlacementFailure::TooManyPieces));
}

TEST_CASE_FIXTURE(Pieces, "undo takes back the newest piece but never the chassis") {
    BuiltCar build = WithChassis();
    REQUIRE(build.Place(brick, {.x = 3}, 0, 0).has_value());
    REQUIRE(build.Place(wide, {.x = 3, .rotation = 5}, 9, 2).has_value());

    const CarPiece newest = build.Undo().value_or(CarPiece{});

    CHECK(newest.piece == &wide);
    CHECK(newest.placement == PiecePlacement{.x = 3, .rotation = 1});
    CHECK(newest.colour == 9);
    CHECK(newest.partSet == 2);
    CHECK(build.Grid().At(3, 1).height == 1);
    CHECK(build.Grid().At(3, 0).height == 4);
    CHECK(build.Undo().has_value());
    CHECK_FALSE(build.Undo().has_value());
    CHECK(build.Pieces().size() == 1);
}

TEST_CASE_FIXTURE(Pieces, "the brick volume counts plates and leaves out the chassis") {
    BuiltCar build = WithChassis();
    CHECK(build.Bricks().cells == 0);
    REQUIRE(build.Place(brick, {.x = 2, .y = 1}, 0, 0).has_value());
    REQUIRE(build.Place(brick, {.x = 7, .y = 4}, 0, 0).has_value());

    const BrickVolume bricks = build.Bricks();

    CHECK(bricks.cells == 6);
    CHECK(bricks.centroid.x == doctest::Approx(0.0F));
    CHECK(bricks.centroid.y == doctest::Approx(0.0F));
    CHECK(bricks.centroid.z == doctest::Approx(2.0F));
}

}
}
