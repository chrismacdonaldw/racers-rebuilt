#include "app/menu/car_builder.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>

#include "app/menu/car_edit.hpp"
#include "assets/ldraw_extra_pieces.hpp"
#include "sim/car_build/car_pieces.hpp"
#include "sim/car_build/retail_cars.hpp"

namespace rr::app {
namespace {

using sim::test::Block;

constexpr std::size_t kView = 1;
constexpr std::size_t kSetCount = 12;
constexpr std::size_t kTechnicSet = 3;
constexpr std::uint16_t kTechnicChassis = 18;

struct Pieces {
    assets::PieceCatalog catalog = Catalog();

    static assets::PieceCatalog Catalog() {
        assets::PieceCatalog catalog;
        catalog.library.pieces = {sim::test::Chassis(), Block(0x900, 1, 1, 3),
                                  Block(0x901, 2, 1, 3), Block(kTechnicChassis, 10, 6, 1)};
        catalog.partSets.resize(kSetCount);
        catalog.partSets.at(kTechnicSet).chassis = 3;
        catalog.partSets.at(0).choices = {{.piece = 1, .colour = 1}};
        catalog.partSets.at(kTechnicSet).choices = {{.piece = 2, .colour = 2},
                                                    {.piece = 1, .colour = 5}};
        return catalog;
    }

    [[nodiscard]] CarBuilder Builder() const {
        sim::BuiltCar car;
        REQUIRE(car.Place(catalog.library.pieces.front(), {}, 4, 0).has_value());
        return {catalog, car, 0};
    }
};

int Distance(sim::PiecePlacement from, sim::PiecePlacement to) {
    return std::abs(to.x - from.x) + std::abs(to.y - from.y);
}

std::size_t StepToward(const CarBuilder& builder, sim::PiecePlacement target) {
    std::size_t best = 0;
    int bestDistance = Distance(builder.Cursor().Placement(), target);
    for (std::size_t direction = 0; direction < kBuildDirections; ++direction) {
        CarBuilder moved = builder;
        moved.Move(direction, kView);
        const int distance = Distance(moved.Cursor().Placement(), target);
        if (distance < bestDistance) {
            best = direction;
            bestDistance = distance;
        }
    }
    return best;
}

void SteerTo(CarBuilder& builder, sim::PiecePlacement target) {
    for (int turn = 0; turn < 4 && builder.Cursor().Placement().rotation != target.rotation;
         ++turn) {
        builder.Rotate();
    }
    for (int step = 0; step < 20 && builder.Cursor().Placement() != target; ++step) {
        builder.Move(StepToward(builder, target), kView);
    }
}

std::size_t ChoiceOf(const assets::PieceCatalog& catalog, std::size_t set,
                     const sim::CarPiece& piece) {
    const std::span<const assets::PartChoice> choices = catalog.partSets.at(set).choices;
    const auto found = std::ranges::find_if(choices, [&](const assets::PartChoice& choice) {
        return catalog.library.pieces.at(choice.piece).type == piece.piece->type &&
               choice.colour == piece.colour;
    });
    REQUIRE(found != choices.end());
    return static_cast<std::size_t>(found - choices.begin());
}

CarBuilder OnChassis(const assets::PieceCatalog& catalog, const sim::CarPiece& base) {
    sim::BuiltCar chassis;
    REQUIRE(chassis.Place(*base.piece, base.placement, base.colour, base.partType).has_value());
    return {catalog, chassis, 0};
}

void Rebuild(CarBuilder& builder, const assets::PieceCatalog& catalog, const sim::CarPiece& piece) {
    const std::optional<std::size_t> set = PartSetOfType(catalog, piece.partType);
    REQUIRE(set.has_value());
    builder.SelectSet(*set);
    builder.Choose(ChoiceOf(catalog, *set, piece));
    SteerTo(builder, piece.placement);
    REQUIRE(builder.Place() == PlaceResult::Placed);
}

}

TEST_CASE_FIXTURE(Pieces, "the builder starts holding the set's first choice") {
    const CarBuilder builder = Builder();

    CHECK(builder.Cursor().Piece() == &catalog.library.pieces.at(1));
    CHECK(builder.Colour() == 1);
    CHECK(builder.Cursor().Placement() == sim::PiecePlacement{.x = 4, .y = 2});
}

TEST_CASE_FIXTURE(Pieces, "arrows move the piece by the camera's view") {
    CarBuilder builder = Builder();

    CHECK(builder.Move(3, kView));
    CHECK(builder.Cursor().Placement() == sim::PiecePlacement{.x = 4, .y = 3});
    CHECK(builder.Move(4, kView));
    CHECK(builder.Cursor().Placement() == sim::PiecePlacement{.x = 5, .y = 4});
    CHECK(builder.Move(2, 0));
    CHECK(builder.Cursor().Placement() == sim::PiecePlacement{.x = 5, .y = 5});
}

TEST_CASE_FIXTURE(Pieces, "a blocked straight move undoes its first half the original's way") {
    CarBuilder builder = Builder();
    for (int step = 0; step < 4; ++step) {
        builder.Move(0, 0);
    }
    REQUIRE(builder.Cursor().Placement() == sim::PiecePlacement{.x = 0, .y = 2});

    CHECK_FALSE(builder.Move(0, kView));

    CHECK(builder.Cursor().Placement() == sim::PiecePlacement{.x = 1, .y = 2});
}

TEST_CASE_FIXTURE(Pieces, "a placed piece keeps the selected set's chassis type") {
    CarBuilder builder = Builder();
    builder.SelectSet(kTechnicSet);
    builder.Choose(1);

    REQUIRE(builder.Place() == PlaceResult::Placed);

    const sim::CarPiece& placed = builder.Car().Pieces().back();
    CHECK(placed.partType == kTechnicChassis);
    CHECK(placed.colour == 5);
    CHECK(builder.Choice() == 1);
}

TEST_CASE_FIXTURE(Pieces, "an extra piece places with no part type and loads back unchanged") {
    catalog.library.pieces.push_back(Block(assets::kFirstExtraPieceType, 2, 2, 1));
    catalog.extras = {{.piece = catalog.library.pieces.size() - 1}};
    CarBuilder builder = Builder();
    builder.Pick(catalog.ExtraSet(), 0, 7);
    builder.PutAt({.x = 3, .y = 2});

    REQUIRE(builder.Place() == PlaceResult::Placed);

    const sim::CarPiece& placed = builder.Car().Pieces().back();
    CHECK(placed.partType == 0);
    CHECK(placed.colour == 7);
    const assets::CarData saved = builder.Car().Save();
    const Result<sim::BuiltCar> loaded = sim::BuiltCar::Load(saved, catalog.library);
    REQUIRE(loaded.has_value());
    CHECK(loaded->Save() == saved);
    CHECK(loaded->Pieces().back().piece->type == assets::kFirstExtraPieceType);
}

TEST_CASE_FIXTURE(Pieces, "undo reselects the set and choice the piece came from") {
    CarBuilder builder = Builder();
    builder.SelectSet(kTechnicSet);
    builder.Choose(1);
    REQUIRE(builder.Place() == PlaceResult::Placed);
    builder.SelectSet(0);
    REQUIRE(builder.Undo());
    CHECK(builder.Set() == kTechnicSet);
    CHECK(builder.Choice() == 1);

    builder.SelectSet(0);
    builder.Move(2, 0);
    REQUIRE(builder.Place() == PlaceResult::Placed);
    builder.SelectSet(kTechnicSet);
    REQUIRE(builder.Undo());

    CHECK(builder.Set() == 0);
    CHECK(builder.Choice() == 0);
    CHECK(builder.Cursor().Piece() == &catalog.library.pieces.at(1));
    CHECK(builder.Colour() == 1);
    CHECK_FALSE(builder.Undo());
}

TEST_CASE_FIXTURE(Pieces, "the 65th piece is refused as too many") {
    sim::BuiltCar car;
    REQUIRE(car.Place(catalog.library.pieces.front(), {}, 4, 0).has_value());
    for (int cell = 0; car.Pieces().size() < sim::kMaxCarPieces; ++cell) {
        const sim::PiecePlacement at{.x = cell % sim::kCarGridWidth,
                                     .y = (cell / sim::kCarGridWidth) % sim::kCarGridDepth};
        REQUIRE(car.Place(catalog.library.pieces.at(1), at, 1, 0).has_value());
    }
    CarBuilder builder{catalog, car, 0};

    CHECK(builder.Place() == PlaceResult::TooManyPieces);
    CHECK(builder.Car().Pieces().size() == sim::kMaxCarPieces);
}

TEST_CASE("the retail JOAN OF KART can be rebuilt through the builder") {
    const std::optional<sim::test::RetailCars> cars =
        sim::test::LoadRetailCars(assets::PieceDetail::Menu);
    if (!cars) {
        return;
    }
    const auto joan = std::ranges::find_if(cars->racers, [](const assets::SavedRacer& racer) {
        return racer.name == u"JOAN OF KART";
    });
    REQUIRE(joan != cars->racers.end());
    const Result<sim::BuiltCar> target = sim::BuiltCar::Load(joan->car, cars->catalog.library);
    REQUIRE(target.has_value());
    CarBuilder builder = OnChassis(cars->catalog, target->Pieces().front());

    for (const sim::CarPiece& piece : target->Pieces().subspan(1)) {
        Rebuild(builder, cars->catalog, piece);
    }

    CHECK(builder.Car().Save() == joan->car);
}

}
