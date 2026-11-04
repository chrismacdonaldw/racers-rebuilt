#include "sim/car_build/build_history.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <expected>
#include <optional>

#include "sim/car_build/car_pieces.hpp"
#include "sim/car_build/retail_cars.hpp"

namespace rr::sim {
namespace {

struct Pieces {
    assets::PieceRecord chassis = test::Chassis();
    assets::PieceRecord brick = test::Block(0x900, 1, 1, 3);
    assets::PieceRecord plate = test::Block(0x902, 1, 1, 1);

    [[nodiscard]] BuiltCar Started() const {
        BuiltCar build;
        REQUIRE(build.Place(chassis, {}, 4, 3).has_value());
        REQUIRE(build.Place(brick, {.x = 2}, 1, 0).has_value());
        REQUIRE(build.Place(brick, {.x = 2}, 2, 0).has_value());
        REQUIRE(build.Place(plate, {.x = 6, .y = 3}, 3, 0).has_value());
        return build;
    }
};

PlacePiece Placing(const assets::PieceRecord& piece, PiecePlacement at) {
    return {.piece = {.piece = &piece, .placement = at, .colour = 7}};
}

std::optional<PlacementFailure> FailureOf(const std::expected<void, PlacementFailure>& done) {
    return done ? std::nullopt : std::optional(done.error());
}

bool UndoesAndRedoes(BuiltCar& car, const BuildEdit& edit) {
    const assets::CarData before = car.Save();
    BuildHistory history;
    if (!history.Do(car, edit)) {
        return false;
    }
    const assets::CarData after = car.Save();
    return after != before && history.Undo(car) && car.Save() == before && history.Redo(car) &&
           car.Save() == after && history.Undo(car) && car.Save() == before;
}

TEST_CASE_FIXTURE(Pieces, "every kind of edit undoes and redoes exactly") {
    BuiltCar car = Started();

    CHECK(UndoesAndRedoes(car, Placing(plate, {.x = 2})));
    CHECK(UndoesAndRedoes(car, RemovePiece{.index = 2}));
    CHECK(UndoesAndRedoes(car, RemovePiece{.index = 3}));
    CHECK(UndoesAndRedoes(car, MovePiece{.index = 2, .placement = {.x = 6, .y = 3}}));
    CHECK(UndoesAndRedoes(car, MovePiece{.index = 3, .placement = {.x = 2}}));
    CHECK(UndoesAndRedoes(car, RecolourPiece{.index = 1, .colour = 9}));
    CHECK(car.Grid().At(2, 0).piece == 2);
}

TEST_CASE_FIXTURE(Pieces, "a refused edit is neither applied nor recorded") {
    BuiltCar car = Started();
    BuildHistory history;

    CHECK(FailureOf(history.Do(car, RemovePiece{.index = 1})) == PlacementFailure::HoldsPieces);
    CHECK(FailureOf(history.Do(car, Placing(chassis, {}))) == PlacementFailure::Chassis);
    CHECK(FailureOf(history.Do(car, RecolourPiece{.index = 9})) == PlacementFailure::NoPiece);
    CHECK(FailureOf(history.Do(car, MovePiece{.index = 3, .placement = {.x = 10}})) ==
          PlacementFailure::PastRight);
    CHECK_FALSE(history.CanUndo());
    CHECK(car.Pieces().size() == 4);
}

TEST_CASE_FIXTURE(Pieces, "a new edit forgets what was undone") {
    BuiltCar car = Started();
    BuildHistory history;
    REQUIRE(history.Do(car, RecolourPiece{.index = 1, .colour = 9}));
    REQUIRE(history.Undo(car));
    REQUIRE(history.CanRedo());

    REQUIRE(history.Do(car, RecolourPiece{.index = 3, .colour = 5}));

    CHECK_FALSE(history.CanRedo());
    CHECK_FALSE(history.Redo(car));
    CHECK(car.Pieces()[1].colour == 1);
}

TEST_CASE_FIXTURE(Pieces, "the history keeps the newest 256 steps") {
    BuiltCar car = Started();
    BuildHistory history;
    for (std::size_t step = 0; step < kBuildHistorySteps + 10; ++step) {
        REQUIRE(history.Do(car, RecolourPiece{.index = 1, .colour = 9}));
    }

    std::size_t undone = 0;
    while (history.Undo(car)) {
        ++undone;
    }

    CHECK(undone == kBuildHistorySteps);
}

std::size_t RemoveTopPieces(BuiltCar& car, BuildHistory& history) {
    std::size_t removed = 0;
    for (std::size_t index = car.Pieces().size(); index > 1; --index) {
        removed += history.Do(car, RemovePiece{.index = index - 1}) ? 1 : 0;
    }
    return removed;
}

std::size_t TakeApart(BuiltCar& car, BuildHistory& history) {
    const std::size_t pieces = car.Pieces().size();
    std::size_t removed = 0;
    for (std::size_t pass = 0; pass < pieces && car.Pieces().size() > 1; ++pass) {
        removed += RemoveTopPieces(car, history);
    }
    return removed;
}

std::size_t UndoAll(BuiltCar& car, BuildHistory& history) {
    std::size_t undone = 0;
    while (history.Undo(car)) {
        ++undone;
    }
    return undone;
}

TEST_CASE("the retail JOAN OF KART comes apart top down and undo rebuilds it identically") {
    const std::optional<test::RetailCars> cars = test::LoadRetailCars(assets::PieceDetail::Menu);
    if (!cars) {
        return;
    }
    const assets::CarData& original = cars->racers.front().car;
    Result<BuiltCar> joan = BuiltCar::Load(original, cars->catalog.library);
    REQUIRE(joan.has_value());
    const std::size_t pieces = joan->Pieces().size();
    BuildHistory history;

    const std::size_t removed = TakeApart(*joan, history);
    CHECK(removed == pieces - 1);
    CHECK(UndoAll(*joan, history) == removed);
    CHECK(joan->Save() == original);
}

}
}
