#include "sim/car_build/bench.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

#include "sim/car_build/car_pieces.hpp"

namespace rr::sim {
namespace {

struct Pieces {
    assets::PieceRecord chassis = test::Chassis();
    assets::PieceRecord brick = test::Block(0x900, 1, 1, 3);
    assets::PieceRecord plate = test::Block(0x902, 1, 1, 1);

    [[nodiscard]] Bench Started() const {
        Bench bench;
        REQUIRE(bench.car.Place(chassis, {}, 4, 3).has_value());
        REQUIRE(bench.car.Place(brick, {.x = 2}, 1, 17).has_value());
        REQUIRE(bench.car.Place(plate, {.x = 2}, 2, 17).has_value());
        return bench;
    }
};

struct State {
    assets::CarData car{};
    std::size_t loose = 0;
    std::optional<CarPiece> firstLoose;

    friend bool operator==(const State& x, const State& y) {
        const auto same = [](const std::optional<CarPiece>& a, const std::optional<CarPiece>& b) {
            return a.has_value() == b.has_value() &&
                   (!a || (a->piece == b->piece && a->placement == b->placement &&
                           a->height == b->height && a->colour == b->colour &&
                           a->partType == b->partType));
        };
        return x.car == y.car && x.loose == y.loose && same(x.firstLoose, y.firstLoose);
    }
};

State StateOf(const Bench& bench) {
    const auto loose = bench.ground.Pieces();
    return {.car = bench.car.Save(),
            .loose = loose.size(),
            .firstLoose = loose.empty() ? std::nullopt : std::optional(loose.front())};
}

std::optional<PlacementFailure> FailureOf(const std::expected<void, PlacementFailure>& done) {
    return done ? std::nullopt : std::optional(done.error());
}

bool UndoesAndRedoes(Bench& bench, const BenchEdit& edit) {
    const State before = StateOf(bench);
    BenchHistory history;
    if (!history.Do(bench, edit)) {
        return false;
    }
    const State after = StateOf(bench);
    return !(after == before) && history.Undo(bench) && StateOf(bench) == before &&
           history.Redo(bench) && StateOf(bench) == after && history.Undo(bench) &&
           StateOf(bench) == before;
}

TEST_CASE_FIXTURE(Pieces, "every kind of edit undoes and redoes as one step") {
    Bench bench = Started();
    BenchHistory history;
    REQUIRE(history
                .Do(bench, PutPiece{.area = BenchArea::Ground,
                                    .piece = {.piece = &brick, .placement = {.x = 1}}})
                .has_value());

    CHECK(UndoesAndRedoes(bench, PutPiece{.area = BenchArea::Car,
                                          .piece = {.piece = &plate, .placement = {.x = 5}}}));
    CHECK(UndoesAndRedoes(bench, LiftPiece{.area = BenchArea::Car, .index = 2}));
    CHECK(UndoesAndRedoes(bench, LiftPiece{.area = BenchArea::Ground, .index = 0}));
    CHECK(UndoesAndRedoes(bench, ShiftPiece{.from = BenchArea::Car,
                                            .index = 2,
                                            .to = BenchArea::Car,
                                            .placement = {.x = 7, .y = 4}}));
    CHECK(UndoesAndRedoes(bench, ShiftPiece{.from = BenchArea::Ground,
                                            .index = 0,
                                            .to = BenchArea::Ground,
                                            .placement = {.x = 3, .y = 3}}));
    CHECK(UndoesAndRedoes(bench, ShiftPiece{.from = BenchArea::Car,
                                            .index = 2,
                                            .to = BenchArea::Ground,
                                            .placement = {.x = 5, .y = 5}}));
    CHECK(UndoesAndRedoes(bench, ShiftPiece{.from = BenchArea::Ground,
                                            .index = 0,
                                            .to = BenchArea::Car,
                                            .placement = {.x = 8, .y = 1}}));
    CHECK(UndoesAndRedoes(bench, PaintPiece{.area = BenchArea::Car, .index = 1, .colour = 9}));
    CHECK(UndoesAndRedoes(bench, PaintPiece{.area = BenchArea::Ground, .index = 0, .colour = 9}));
}

TEST_CASE_FIXTURE(Pieces, "a piece keeps its colour and set going to the ground and back") {
    Bench bench = Started();
    BenchHistory history;

    REQUIRE(history
                .Do(bench, ShiftPiece{.from = BenchArea::Car,
                                      .index = 2,
                                      .to = BenchArea::Ground,
                                      .placement = {.x = 4, .y = 4}})
                .has_value());
    REQUIRE(bench.car.Pieces().size() == 2);
    REQUIRE(bench.ground.Pieces().size() == 1);
    CHECK(bench.ground.Pieces()[0].colour == 2);

    REQUIRE(history
                .Do(bench, ShiftPiece{.from = BenchArea::Ground,
                                      .index = 0,
                                      .to = BenchArea::Car,
                                      .placement = {.x = 6, .y = 2}})
                .has_value());
    REQUIRE(bench.car.Pieces().size() == 3);
    CHECK(bench.ground.Pieces().empty());
    CHECK(bench.car.Pieces()[2].colour == 2);
    CHECK(bench.car.Pieces()[2].partType == 17);
    CHECK(bench.car.Pieces()[2].height == 1);
}

TEST_CASE_FIXTURE(Pieces, "a refused edit changes nothing and isn't kept") {
    Bench bench = Started();
    BenchHistory history;
    REQUIRE(history
                .Do(bench, PutPiece{.area = BenchArea::Ground,
                                    .piece = {.piece = &brick, .placement = {.x = 1}}})
                .has_value());
    const State before = StateOf(bench);

    CHECK(FailureOf(history.Do(bench, ShiftPiece{.from = BenchArea::Car,
                                                 .index = 1,
                                                 .to = BenchArea::Ground,
                                                 .placement = {.x = 3}})) ==
          PlacementFailure::HoldsPieces);
    CHECK(FailureOf(history.Do(bench, ShiftPiece{.from = BenchArea::Car,
                                                 .index = 0,
                                                 .to = BenchArea::Ground,
                                                 .placement = {.x = 3}})) ==
          PlacementFailure::Chassis);
    CHECK(FailureOf(history.Do(bench, ShiftPiece{.from = BenchArea::Ground,
                                                 .index = 0,
                                                 .to = BenchArea::Car,
                                                 .placement = {.x = 20}})) ==
          PlacementFailure::PastRight);
    CHECK(FailureOf(
              history.Do(bench, PutPiece{.area = BenchArea::Car, .piece = {.piece = &chassis}})) ==
          PlacementFailure::Chassis);
    CHECK(FailureOf(history.Do(bench, PaintPiece{.area = BenchArea::Ground, .index = 4})) ==
          PlacementFailure::NoPiece);
    CHECK(StateOf(bench) == before);

    CHECK(history.Undo(bench));
    CHECK(bench.ground.Pieces().empty());
    CHECK(!history.CanUndo());
}

TEST_CASE_FIXTURE(Pieces, "a new edit forgets what was undone") {
    Bench bench = Started();
    BenchHistory history;
    REQUIRE(history.Do(bench, LiftPiece{.area = BenchArea::Car, .index = 2}).has_value());
    REQUIRE(history.Undo(bench));
    REQUIRE(history.CanRedo());

    REQUIRE(
        history.Do(bench, PaintPiece{.area = BenchArea::Car, .index = 2, .colour = 8}).has_value());

    CHECK(!history.CanRedo());
    CHECK(!history.Redo(bench));
}

TEST_CASE_FIXTURE(Pieces, "the history keeps the newest steps") {
    Bench bench = Started();
    BenchHistory history;
    for (std::size_t step = 0; step < kBenchHistorySteps + 4; ++step) {
        REQUIRE(history
                    .Do(bench, PaintPiece{.area = BenchArea::Car,
                                          .index = 1,
                                          .colour = static_cast<std::uint8_t>(step)})
                    .has_value());
    }

    std::size_t undone = 0;
    while (history.Undo(bench)) {
        ++undone;
    }

    CHECK(undone == kBenchHistorySteps);
    CHECK(bench.car.Pieces()[1].colour == 3);
}

}
}
