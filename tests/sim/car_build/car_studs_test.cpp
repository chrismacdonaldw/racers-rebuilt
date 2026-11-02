#include "sim/car_build/car_studs.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "sim/car_build/car_pieces.hpp"

namespace rr::sim {
namespace {

using test::Block;
using test::Cell;
using test::Piece;

constexpr std::uint8_t kChassisColour = 4;
constexpr std::uint8_t kBrickColour = 7;
constexpr std::size_t kChassisTops = 60;

struct Pieces {
    assets::PieceRecord chassis = test::Chassis();
    assets::PieceRecord brick = Block(0x900, 1, 1, 3);

    [[nodiscard]] BuiltCar WithChassis() const {
        BuiltCar car;
        REQUIRE(car.Place(chassis, {}, kChassisColour, 0).has_value());
        return car;
    }
};

CarStud ChassisStud(int x, int y) {
    return {.x = x, .y = y, .height = 1, .colour = kChassisColour};
}

TEST_CASE_FIXTURE(Pieces, "every occupied top has a stud in its piece's colour") {
    BuiltCar car = WithChassis();
    REQUIRE(car.Place(brick, {.x = 2, .y = 1}, kBrickColour, 0).has_value());

    const std::vector<CarStud> studs = CarStuds(car);

    REQUIRE(studs.size() == kChassisTops);
    CHECK(studs.front() == ChassisStud(0, 0));
    CHECK(std::ranges::count(studs, CarStud{.x = 2, .y = 1, .height = 4, .colour = kBrickColour}) ==
          1);
}

TEST_CASE_FIXTURE(Pieces, "a top a piece covers keeps its stud when the piece doesn't sit on it") {
    const assets::PieceRecord bridge = Piece(0x903, 2, 1, {Cell(3), Cell(3, 0)});
    BuiltCar car = WithChassis();
    REQUIRE(car.Place(brick, {}, kBrickColour, 0).has_value());
    REQUIRE(car.Place(bridge, {}, kBrickColour, 0) == 4);

    const std::vector<CarStud> studs = CarStuds(car);

    CHECK(studs.size() == kChassisTops + 1);
    CHECK(studs.back() == ChassisStud(1, 0));
}

TEST_CASE_FIXTURE(Pieces, "a top under a hollow cell keeps its stud") {
    const assets::PieceRecord hollow = Piece(0x904, 1, 1, {Cell(3, 0xC0)});
    BuiltCar car = WithChassis();
    REQUIRE(car.Place(hollow, {.x = 5, .y = 2}, kBrickColour, 0) == 1);

    const std::vector<CarStud> studs = CarStuds(car);

    CHECK(studs.size() == kChassisTops + 1);
    CHECK(studs.back() == ChassisStud(5, 2));
}

TEST_CASE("a chassis cell's raised bottom goes with its stud") {
    assets::PieceRecord chassis = test::Chassis();
    chassis.shape.cells[7] = {.top = 0x81, .bottom = 0x80 | 12};
    BuiltCar car;
    REQUIRE(car.Place(chassis, {}, kChassisColour, 0).has_value());

    const std::vector<CarStud> studs = CarStuds(car);

    REQUIRE(studs.size() == kChassisTops);
    CHECK(studs[7].raisedBottom == 11);
    CHECK(studs[8].raisedBottom == 0);
}

}
}
