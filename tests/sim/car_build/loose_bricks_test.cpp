#include "sim/car_build/loose_bricks.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <expected>
#include <optional>
#include <vector>

#include "sim/car_build/car_pieces.hpp"

namespace rr::sim {
namespace {

struct Pieces {
    assets::PieceRecord brick = test::Block(0x900, 2, 1, 3);
    assets::PieceRecord plate = test::Block(0x902, 1, 1, 1);
    assets::PieceRecord chassis = test::Chassis();
};

CarPiece At(const assets::PieceRecord& piece, int x, int y) {
    return {.piece = &piece, .placement = {.x = x, .y = y}, .colour = 5};
}

std::optional<PlacementFailure> FailureOf(const std::expected<int, PlacementFailure>& laid) {
    return laid ? std::nullopt : std::optional(laid.error());
}

TEST_CASE_FIXTURE(Pieces, "pieces lie on the ground and stack on each other") {
    LooseBricks ground;

    CHECK(ground.Put(At(brick, 2, 2)) == 0);
    CHECK(ground.Put(At(plate, 3, 2)) == 3);
    CHECK(ground.Put(At(plate, 3, 2)) == 4);
    CHECK(ground.Pieces().size() == 3);
    CHECK(ground.Pieces()[2].height == 4);
    CHECK(ground.Pieces()[2].colour == 5);
}

TEST_CASE_FIXTURE(Pieces, "the cells under the car's grid stay clear") {
    LooseBricks ground;

    CHECK(FailureOf(ground.Put(At(plate, kCarOnGround.x, kCarOnGround.y))) ==
          PlacementFailure::Collision);
    CHECK(FailureOf(ground.Put(At(brick, kCarOnGround.x - 1, kCarOnGround.y))) ==
          PlacementFailure::Collision);
    CHECK(ground.Put(At(brick, kCarOnGround.x - 2, kCarOnGround.y)) == 0);
    CHECK(ground.Put(At(plate, kCarOnGround.x + kCarGridWidth, kCarOnGround.y)) == 0);
    CHECK(ground.Put(At(plate, kCarOnGround.x, kCarOnGround.y + kCarGridDepth)) == 0);
}

TEST_CASE_FIXTURE(Pieces, "the ground has edges and takes no chassis") {
    LooseBricks ground;

    CHECK(FailureOf(ground.Put(At(brick, kGroundSize.width - 1, 0))) ==
          PlacementFailure::PastRight);
    CHECK(FailureOf(ground.Put(At(plate, 0, -1))) == PlacementFailure::OffLeftOrTop);
    CHECK(FailureOf(ground.Put(At(chassis, 0, 0))) == PlacementFailure::Chassis);
}

TEST_CASE_FIXTURE(Pieces, "a stack on the ground stops at the car's height limit") {
    LooseBricks ground;
    for (int level = 0; level < kMaxCarHeight; ++level) {
        REQUIRE(ground.Put(At(plate, 0, 0)).has_value());
    }

    CHECK(FailureOf(ground.Put(At(plate, 0, 0))) == PlacementFailure::TooHigh);
}

TEST_CASE_FIXTURE(Pieces, "the ground holds a limited number of pieces") {
    LooseBricks ground;
    for (std::size_t count = 0; count < kMaxLoosePieces; ++count) {
        const auto cell = static_cast<int>(count % 8);
        REQUIRE(ground.Put(At(plate, cell, static_cast<int>(count / 64))).has_value());
    }

    CHECK(FailureOf(ground.Put(At(plate, 9, 9))) == PlacementFailure::TooManyPieces);
}

TEST_CASE_FIXTURE(Pieces, "taking a piece lets the ones above it settle") {
    LooseBricks ground;
    REQUIRE(ground.Put(At(brick, 2, 2)).has_value());
    REQUIRE(ground.Put(At(plate, 3, 2)).has_value());
    REQUIRE(ground.Put(At(plate, 3, 2)).has_value());

    const std::expected<CarPiece, PlacementFailure> taken = ground.Take(0);

    REQUIRE(taken.has_value());
    CHECK(taken.value_or(CarPiece{}).piece == &brick);
    REQUIRE(ground.Pieces().size() == 2);
    CHECK(ground.Pieces()[0].height == 0);
    CHECK(ground.Pieces()[1].height == 1);
    CHECK(ground.PieceAt(3, 2, 1) == 1);
    CHECK(ground.PieceAt(2, 2, 0) == std::nullopt);
    CHECK(!ground.Take(2).has_value());
}

TEST_CASE_FIXTURE(Pieces, "a refused move leaves every piece where it was") {
    LooseBricks ground;
    REQUIRE(ground.Put(At(brick, 2, 2)).has_value());
    REQUIRE(ground.Put(At(plate, 3, 2)).has_value());

    CHECK(FailureOf(ground.Move(0, {.x = kCarOnGround.x, .y = kCarOnGround.y})) ==
          PlacementFailure::Collision);
    REQUIRE(ground.Pieces().size() == 2);
    CHECK(ground.Pieces()[0].piece == &brick);
    CHECK(ground.Pieces()[1].height == 3);

    CHECK(ground.Move(0, {.x = 6, .y = 6, .rotation = 5}) == 0);
    CHECK(ground.Pieces()[0].height == 0);
    CHECK(ground.Pieces()[1].placement == PiecePlacement{.x = 6, .y = 6, .rotation = 1});
}

TEST_CASE_FIXTURE(Pieces,
                  "laying pieces down again keeps their order and drops any off the ground") {
    const std::vector<CarPiece> pieces{At(brick, 2, 2), At(plate, 40, 0), At(plate, 3, 2)};

    const LooseBricks ground(pieces);

    REQUIRE(ground.Pieces().size() == 2);
    CHECK(ground.Pieces()[1].height == 3);
}

}
}
