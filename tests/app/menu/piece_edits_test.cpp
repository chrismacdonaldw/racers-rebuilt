#include "app/menu/piece_edits.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "app/menu/build_sounds.hpp"
#include "sim/car_build/car_pieces.hpp"

namespace rr::app {
namespace {

constexpr std::uint8_t kBlockColour = 1;
constexpr sim::PiecePlacement kTower{.x = 2, .y = 2};

struct Tower {
    assets::PieceCatalog catalog = Catalog();
    CarBuilder builder = Built(catalog);

    static assets::PieceCatalog Catalog() {
        assets::PieceCatalog catalog;
        catalog.library.pieces = {sim::test::Chassis(), sim::test::Block(0x900, 1, 1, 3)};
        catalog.partSets.resize(1);
        catalog.partSets.at(0).choices = {{.piece = 1, .colour = kBlockColour}};
        return catalog;
    }

    static CarBuilder Built(const assets::PieceCatalog& catalog) {
        sim::BuiltCar car;
        REQUIRE(car.Place(catalog.library.pieces.front(), {}, 4, 0).has_value());
        CarBuilder builder(catalog, car, 0);
        builder.RecordEdits(true);
        for (int block = 0; block < 2; ++block) {
            builder.Pick(0, 0, kBlockColour);
            builder.PutAt(kTower);
            REQUIRE(builder.Place() == PlaceResult::Placed);
        }
        builder.Release();
        return builder;
    }

    [[nodiscard]] std::size_t Count() const { return builder.Car().Pieces().size(); }
};

TEST_CASE("pieces come off the top of a tower first and undo and redo step through it") {
    Tower tower;
    const EditOutcome blocked = RemoveSelected(tower.builder, 1);
    CHECK(blocked.sound == kBuildRefusedSound);
    CHECK_FALSE(blocked.isChanged);
    CHECK(blocked.blockers == std::vector<std::size_t>{2});
    CHECK(tower.Count() == 3);

    CHECK(RemoveSelected(tower.builder, 2).sound == kBuildUndoSound);
    CHECK(tower.Count() == 2);
    CHECK(StepHistory(tower.builder, PieceEditKey::Undo).isChanged);
    CHECK(tower.Count() == 3);
    CHECK(StepHistory(tower.builder, PieceEditKey::Redo).sound == kBuildPlacedSound);
    CHECK(tower.Count() == 2);
}

TEST_CASE("the chassis never comes off and an empty history refuses") {
    Tower tower;
    const EditOutcome chassis = RemoveSelected(tower.builder, 0);
    CHECK(chassis.sound == kBuildRefusedSound);
    CHECK(chassis.blockers.empty());
    CHECK(tower.Count() == 3);

    CHECK(StepHistory(tower.builder, PieceEditKey::Redo).sound == kBuildRefusedSound);
    tower.builder.RecordEdits(true);
    CHECK(StepHistory(tower.builder, PieceEditKey::Undo).sound == kBuildRefusedSound);
    CHECK(tower.Count() == 3);
}

TEST_CASE("a lifted piece moves through the history") {
    Tower tower;
    CHECK(LiftSelected(tower.builder, 2).sound == kBuildMoveSound);
    CHECK(tower.builder.IsLifted());
    CHECK(tower.Count() == 2);

    tower.builder.PutAt({.x = 5, .y = 3});
    CHECK(tower.builder.Place() == PlaceResult::Placed);
    CHECK_FALSE(tower.builder.IsLifted());
    CHECK(tower.builder.Cursor().Piece() == nullptr);
    CHECK(tower.builder.Car().Pieces().back().placement == sim::PiecePlacement{.x = 5, .y = 3});

    CHECK(tower.builder.UndoEdit());
    CHECK(tower.builder.Car().Pieces().back().placement == kTower);
    CHECK(tower.builder.Car().Pieces().back().height == 4);
}

TEST_CASE("a lifted piece goes back when let go and comes off when dropped off the car") {
    Tower tower;
    CHECK(LiftSelected(tower.builder, 1).blockers == std::vector<std::size_t>{2});
    CHECK_FALSE(tower.builder.IsLifted());

    REQUIRE(LiftSelected(tower.builder, 2).isChanged);
    tower.builder.Release();
    CHECK(tower.Count() == 3);
    CHECK(tower.builder.Car().Pieces()[2].placement == kTower);

    Tower dropped;
    REQUIRE(LiftSelected(dropped.builder, 2).isChanged);
    CHECK(DropOffCar(dropped.builder).sound == kBuildUndoSound);
    CHECK(dropped.Count() == 2);
    CHECK(dropped.builder.UndoEdit());
    CHECK(dropped.Count() == 3);
}

TEST_CASE("a recolour changes one piece and undoes") {
    Tower tower;
    CHECK(RecolourSelected(tower.builder, 1, kBlockColour).sound == 0);
    CHECK(RecolourSelected(tower.builder, 1, 7).sound == kBuildPlacedSound);
    CHECK(tower.builder.Car().Pieces()[1].colour == 7);
    CHECK(tower.builder.UndoEdit());
    CHECK(tower.builder.Car().Pieces()[1].colour == kBlockColour);
}

TEST_CASE("the original controls place pieces without a history") {
    Tower tower;
    tower.builder.RecordEdits(false);
    tower.builder.Pick(0, 0, kBlockColour);
    tower.builder.PutAt({.x = 6, .y = 1});
    CHECK(tower.builder.Place() == PlaceResult::Placed);
    CHECK_FALSE(tower.builder.UndoEdit());
    CHECK(tower.builder.Cursor().Piece() != nullptr);
}

TEST_CASE("delete, undo and redo keys reach the palette builder") {
    CHECK(PieceEditKeyOf(SDLK_DELETE, SDL_KMOD_NONE) == PieceEditKey::Remove);
    CHECK(PieceEditKeyOf(SDLK_Z, SDL_KMOD_LCTRL) == PieceEditKey::Undo);
    CHECK(PieceEditKeyOf(SDLK_Z, static_cast<SDL_Keymod>(SDL_KMOD_LCTRL | SDL_KMOD_LSHIFT)) ==
          PieceEditKey::Redo);
    CHECK(PieceEditKeyOf(SDLK_Y, SDL_KMOD_RCTRL) == PieceEditKey::Redo);
    CHECK(PieceEditKeyOf(SDLK_MINUS, SDL_KMOD_NONE) == PieceEditKey::Undo);
    CHECK_FALSE(PieceEditKeyOf(SDLK_Z, SDL_KMOD_NONE).has_value());
}

}
}
