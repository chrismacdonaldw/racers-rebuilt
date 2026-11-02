#include "app/menu/race_setup_menu.hpp"

#include <doctest/doctest.h>

#include <format>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/menu_scenes.hpp"
#include "assets/archive_survey.hpp"

namespace rr::app {
namespace {

using sim::MenuAction;

std::vector<CourseChoice> Tracks() {
    return {{.track = "RACEC0R1",
             .circuit = "c0",
             .racers = {{.code = "a"}, {.code = "b"}, {.code = "c"}}},
            {.track = "RACEC0R0", .circuit = "c1", .racers = {{.code = "d"}, {.code = "e"}}}};
}

std::optional<std::string> Press(RaceSetupMenu& menu, std::initializer_list<MenuAction> actions,
                                 const RaceSceneSetup& rules = {
                                     .respawn = {.timeout = Milliseconds{3000}}}) {
    std::optional<std::string> next;
    for (const MenuAction action : actions) {
        sim::InputFrame frame;
        frame.menuPressed.set(sim::InputFrame::Index(action));
        next = menu.Tick(frame, rules);
    }
    return next;
}

TEST_CASE("single race setup builds the race scene name from its choices") {
    RaceSetupMenu menu(Tracks());

    const std::optional<std::string> next =
        Press(menu, {MenuAction::Right, MenuAction::Down, MenuAction::Right, MenuAction::Down,
                     MenuAction::Confirm});

    REQUIRE(next.has_value());
    CHECK(*next == "race:RACEC0R0:respawn=3000:racer=1:circuit=c1");
}

TEST_CASE("changing track keeps the racer within its grid") {
    RaceSetupMenu menu(Tracks());

    static_cast<void>(
        Press(menu, {MenuAction::Down, MenuAction::Left, MenuAction::Up, MenuAction::Right}));

    CHECK(menu.Course().track == "RACEC0R0");
    CHECK(menu.Racer() == 1);
}

TEST_CASE("back leaves for the main menu from any row") {
    RaceSetupMenu menu(Tracks());

    CHECK(Press(menu, {MenuAction::Up, MenuAction::Confirm}) == std::string(kMainMenuScene));
    CHECK(Press(menu, {MenuAction::Down, MenuAction::Back}) == std::string(kMainMenuScene));
}

TEST_CASE("confirming a choice moves on to the next row") {
    RaceSetupMenu menu(Tracks());

    CHECK_FALSE(Press(menu, {MenuAction::Confirm}).has_value());
    CHECK(menu.Focus() == RaceSetupRow::Racer);
}

TEST_CASE("versus setup gives each player a racer and keeps the rules' lap count") {
    RaceSetupMenu menu(Tracks(), 2);

    const std::optional<std::string> next =
        Press(menu,
              {MenuAction::Down, MenuAction::Down, MenuAction::Right, MenuAction::Down,
               MenuAction::Confirm},
              {.respawn = {.timeout = Milliseconds{3000}}, .lapCount = 5});

    CHECK(next == "race:RACEC0R1:respawn=3000:racer=0:circuit=c0:players=2:racer2=2:laps=5");
}

TEST_CASE("single race setup keeps the rules' opponent count") {
    RaceSetupMenu menu(Tracks());

    const std::optional<std::string> next =
        Press(menu, {MenuAction::Down, MenuAction::Down, MenuAction::Confirm},
              {.respawn = {.timeout = Milliseconds{3000}}, .opponents = 1});

    CHECK(next == "race:RACEC0R1:respawn=3000:racer=0:circuit=c0:opponents=1");
}

TEST_CASE("only versus setup stops on player 2's racer") {
    RaceSetupMenu single(Tracks());
    RaceSetupMenu versus(Tracks(), 2);

    static_cast<void>(Press(single, {MenuAction::Down, MenuAction::Down}));
    static_cast<void>(Press(versus, {MenuAction::Down, MenuAction::Down}));

    CHECK(single.Focus() == RaceSetupRow::Race);
    CHECK(versus.Focus() == RaceSetupRow::SecondRacer);
    CHECK(versus.SecondRacer() == 1);
}

TEST_CASE("changing track keeps player 2's racer within its grid") {
    RaceSetupMenu menu(Tracks(), 2);

    static_cast<void>(Press(menu, {MenuAction::Down, MenuAction::Down, MenuAction::Right,
                                   MenuAction::Up, MenuAction::Up, MenuAction::Right}));

    CHECK(menu.Course().track == "RACEC0R0");
    CHECK(menu.SecondRacer() == 1);
}

std::vector<CourseChoice> Circuits() {
    return {{.circuit = "c0", .racers = {{.code = "a"}, {.code = "b"}}},
            {.circuit = "c1", .circuitIndex = 1, .racers = {{.code = "c"}}, .isLocked = true}};
}

TEST_CASE("circuit setup builds the circuit scene name from its first race") {
    RaceSetupMenu menu(Circuits());

    const std::optional<std::string> next =
        Press(menu, {MenuAction::Down, MenuAction::Right, MenuAction::Down, MenuAction::Confirm});

    CHECK(next == "circuit:c0:respawn=3000:racer=1:race=0:points=0,0,0,0,0,0");
}

TEST_CASE("a locked circuit can't start") {
    RaceSetupMenu menu(Circuits());

    const std::optional<std::string> next =
        Press(menu, {MenuAction::Right, MenuAction::Down, MenuAction::Down, MenuAction::Confirm});

    CHECK(menu.Course().circuit == "c1");
    CHECK(menu.Focus() == RaceSetupRow::Race);
    CHECK_FALSE(next.has_value());
}

void CheckRacerNamed(const RacerChoice& racer, const std::vector<std::u16string>& names) {
    CHECK_MESSAGE(racer.nameIndex >= 0, racer.code);
    CHECK_MESSAGE(static_cast<std::size_t>(racer.nameIndex) < names.size(), racer.code);
}

void CheckRacersNamed(const std::vector<CourseChoice>& tracks,
                      const std::vector<std::u16string>& names) {
    for (const CourseChoice& track : tracks) {
        CHECK_MESSAGE(track.racers.size() == 6, track.circuit);
        for (const RacerChoice& racer : track.racers) {
            CheckRacerNamed(racer, names);
        }
    }
}

void CheckTracksNamed(const std::vector<CourseChoice>& tracks,
                      const std::vector<std::u16string>& names) {
    for (const CourseChoice& track : tracks) {
        CHECK_MESSAGE(track.nameLine >= 0, track.track);
        CHECK_MESSAGE(static_cast<std::size_t>(track.nameLine) < names.size(), track.track);
    }
}

TEST_CASE("every unmirrored retail circuit race offers a full grid of named racers") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<std::vector<CourseChoice>> tracks = LoadTrackChoices(*vfs);

    const Result<std::vector<std::u16string>> names = ReadDriverNames(*vfs, "ENGLISH");
    const Result<std::vector<std::u16string>> raceNames = ReadRaceNames(*vfs, "ENGLISH");

    REQUIRE(tracks.has_value());
    REQUIRE(names.has_value());
    REQUIRE(raceNames.has_value());
    CHECK(tracks->size() == 13);
    CheckRacersNamed(*tracks, *names);
    CheckTracksNamed(*tracks, *raceNames);
}

TEST_CASE("single race takes a track's grid from the circuit it's listed under") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<std::vector<CourseChoice>> tracks = LoadTrackChoices(*vfs);

    REQUIRE(tracks.has_value());
    CHECK(tracks->front().track == "RACEC0R1");
    CHECK(tracks->front().circuit == "c0");
    CHECK(tracks->front().racers.at(1).code == "CR");
}

std::vector<std::string> Describe(const std::vector<CourseChoice>& circuits) {
    std::vector<std::string> described;
    described.reserve(circuits.size());
    for (const CourseChoice& circuit : circuits) {
        described.push_back(std::format("{}{} {} {}", circuit.circuit, circuit.track,
                                        circuit.isLocked ? "locked" : "open",
                                        circuit.racers.size()));
    }
    return described;
}

TEST_CASE("circuit setup offers every retail circuit, mirrored ones too, the first always open") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<std::vector<CourseChoice>> circuits = LoadCircuitChoices(*vfs, {"c2", "c4"});

    REQUIRE(circuits.has_value());
    CHECK(Describe(*circuits) == std::vector<std::string>{"c0 open 6", "c1 locked 6", "c2 open 6",
                                                          "c3 locked 6", "c4 open 6", "c5 locked 6",
                                                          "c6 locked 6"});
    CHECK(circuits->back().circuitIndex == 6);
}

}
}
