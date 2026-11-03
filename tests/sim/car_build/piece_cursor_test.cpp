#include "sim/car_build/piece_cursor.hpp"

#include <doctest/doctest.h>

#include "sim/car_build/car_pieces.hpp"

namespace rr::sim {
namespace {

using test::Block;

struct Pieces {
    assets::PieceRecord brick = Block(0x900, 1, 1, 3);
    assets::PieceRecord square = Block(0x901, 2, 2, 3);
    assets::PieceRecord bar = Block(0x902, 3, 1, 3);
    assets::PieceRecord beam = Block(0x903, 8, 1, 3);
    assets::PieceRecord deep = Block(0x904, 4, 8, 3);
};

}

TEST_CASE_FIXTURE(Pieces, "the first piece held starts centred, turned if it's too deep") {
    PieceCursor small;
    small.Hold(square);
    CHECK(small.Placement() == PiecePlacement{.x = 4, .y = 2});

    PieceCursor turned;
    turned.Hold(deep);
    CHECK(turned.Placement() == PiecePlacement{.x = 1, .y = 1, .rotation = 1});
}

TEST_CASE_FIXTURE(Pieces, "moves stop at the grid's edges") {
    PieceCursor cursor;
    cursor.Hold(square);

    CHECK(cursor.MoveAcross(10));
    CHECK(cursor.Placement().x == 8);
    CHECK_FALSE(cursor.MoveAcross(1));
    CHECK(cursor.MoveDeep(-1));
    CHECK(cursor.MoveDeep(-5));
    CHECK(cursor.Placement() == PiecePlacement{.x = 8, .y = 0});
    CHECK_FALSE(cursor.MoveDeep(-1));
}

TEST_CASE_FIXTURE(Pieces, "a new piece keeps to the corner the old one was near") {
    PieceCursor cursor;
    cursor.Hold(square);
    cursor.MoveAcross(4);
    cursor.MoveDeep(2);
    REQUIRE(cursor.Placement() == PiecePlacement{.x = 8, .y = 4});

    cursor.Hold(brick);

    CHECK(cursor.Placement() == PiecePlacement{.x = 9, .y = 5});
}

TEST_CASE_FIXTURE(Pieces, "turning keeps the near corner and stays on the grid") {
    PieceCursor cursor;
    cursor.Hold(bar);
    cursor.MoveAcross(10);
    REQUIRE(cursor.Placement() == PiecePlacement{.x = 7, .y = 2});

    cursor.Rotate();

    CHECK(cursor.Placement() == PiecePlacement{.x = 9, .y = 2, .rotation = 1});
}

TEST_CASE_FIXTURE(Pieces, "a piece too long for the grid's depth skips that turn") {
    PieceCursor cursor;
    cursor.Hold(beam);

    cursor.Rotate();

    CHECK(cursor.Placement() == PiecePlacement{.x = 1, .y = 2, .rotation = 2});
}

TEST_CASE_FIXTURE(Pieces, "an undone piece is put back on the grid") {
    PieceCursor cursor;
    cursor.Hold(brick);

    cursor.PutAt(square, {.x = 9, .y = 5, .rotation = 4});

    CHECK(cursor.Piece() == &square);
    CHECK(cursor.Placement() == PiecePlacement{.x = 8, .y = 4});
}

}
