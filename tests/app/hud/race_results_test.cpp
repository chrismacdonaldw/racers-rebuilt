#include "app/hud/race_results.hpp"

#include <doctest/doctest.h>

#include <array>
#include <string>
#include <vector>

#include "ring_race.hpp"

namespace rr::app {
namespace {

TEST_CASE("results time each finisher against the winner and mark racers still out") {
    test::RingRace race(3, 1);
    race.StartRacing();
    race.DriveLap(2);
    race.DriveLap(2);
    race.DriveLap(0);
    race.Wait(0, Milliseconds{1250});
    race.DriveLap(0);
    race.Step(1, -5.0F, 5.0F);
    const std::array<Vector3, 3> positions{Vector3{.x = -5.0F}, Vector3{.x = 5.0F},
                                           Vector3{.x = -5.0F}};
    race.progress.Rank(race.graph, positions);
    std::vector<ResultRow> rows{{.racer = 9}};

    ReadResults(race.progress, rows);

    REQUIRE(rows.size() == 3);
    CHECK(rows[0].racer == 2);
    CHECK(rows[0].status == ResultStatus::Winner);
    CHECK(rows[1].racer == 0);
    CHECK(rows[1].status == ResultStatus::Behind);
    CHECK(rows[1].behind == Milliseconds{1250});
    CHECK(rows[2].racer == 1);
    CHECK(rows[2].status == ResultStatus::Racing);
}

TEST_CASE("racers given places without finishing still show as racing") {
    test::RingRace race(2, 1);
    race.StartRacing();
    race.DriveLap(1);
    race.DriveLap(1);
    race.progress.FinishRemaining();
    const std::array<Vector3, 2> positions{};
    race.progress.Rank(race.graph, positions);
    std::vector<ResultRow> rows;

    ReadResults(race.progress, rows);

    REQUIRE(rows.size() == 2);
    CHECK(rows[0].racer == 1);
    CHECK(rows[1].status == ResultStatus::Racing);
}

struct Table {
    std::vector<std::u16string> names{u"A", u"B", u"C", u"D", u"E", u"F", u"G"};
    TextPool pool;
    std::vector<HudText> texts;

    explicit Table(std::span<const ResultRow> rows) {
        LayoutResults(rows, names, {.finished = u"FINISHED", .stillRacing = u"STILL RACING"}, pool,
                      texts);
    }
};

TEST_CASE("the results table puts rank, name and status on rows 32 apart from y 132") {
    const std::array<ResultRow, 2> rows{
        ResultRow{.racer = 2, .status = ResultStatus::Winner},
        ResultRow{.racer = 0, .status = ResultStatus::Behind, .behind = Milliseconds{1250}}};

    const Table table(rows);
    const std::vector<HudText>& texts = table.texts;

    REQUIRE(texts.size() == 7);
    CHECK(texts[1].text == u"C");
    CHECK(texts[1].x == 50.0F);
    CHECK(texts[2].text == u"FINISHED");
    CHECK(texts[3].y == 164.0F);
    CHECK(texts[5].text == u"+");
    CHECK(texts[6].text == u"0:01:25");
    CHECK(texts[6].x == 520.0F);
}

TEST_CASE("the results table shows six rows at most") {
    const std::vector<ResultRow> rows(7);

    CHECK(Table(rows).texts.size() == 18);
}

}
}
