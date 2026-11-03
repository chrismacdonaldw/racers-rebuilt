#include "app/menu/race_setup_menu.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/menu/menu_scenes.hpp"
#include "assets/archive_survey.hpp"
#include "assets/folder_layer.hpp"
#include "temp_folder.hpp"

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

TEST_CASE("single race setup picks the player's racer, then races the chosen track") {
    RaceSetupMenu menu(Tracks());

    const std::optional<std::string> next =
        Press(menu, {MenuAction::Right, MenuAction::Down, MenuAction::Confirm});

    REQUIRE(next.has_value());
    CHECK(*next == "menu:pick_racer>race:RACEC0R0:respawn=3000:racer=0:circuit=c1");
}

TEST_CASE("left and right change the course only while it has focus") {
    RaceSetupMenu menu(Tracks());

    static_cast<void>(Press(menu, {MenuAction::Down, MenuAction::Right}));
    CHECK(menu.Course().track == "RACEC0R1");
    static_cast<void>(Press(menu, {MenuAction::Up, MenuAction::Right}));
    CHECK(menu.Course().track == "RACEC0R0");
}
TEST_CASE("a track outside every circuit races on its own") {
    RaceSetupMenu menu(
        {{.track = "MODTRACK", .racers = {{.code = "a"}}, .previews = {std::nullopt}}});

    CHECK(Press(menu, {MenuAction::Down, MenuAction::Confirm}) ==
          std::string("menu:pick_racer>race:MODTRACK:respawn=3000:racer=0"));
}

TEST_CASE("back leaves for the main menu from any row") {
    RaceSetupMenu menu(Tracks());

    CHECK(Press(menu, {MenuAction::Up, MenuAction::Confirm}) == std::string(kMainMenuScene));
    CHECK(Press(menu, {MenuAction::Down, MenuAction::Back}) == std::string(kMainMenuScene));
}

TEST_CASE("confirming a choice moves on to the next row") {
    RaceSetupMenu menu(Tracks());

    CHECK_FALSE(Press(menu, {MenuAction::Confirm}).has_value());
    CHECK(menu.Focus() == RaceSetupRow::Race);
}

TEST_CASE("versus setup has each player pick a racer and keeps the rules' lap count") {
    RaceSetupMenu menu(Tracks(), 2);

    const std::optional<std::string> next =
        Press(menu, {MenuAction::Down, MenuAction::Confirm},
              {.respawn = {.timeout = Milliseconds{3000}}, .lapCount = 5});

    CHECK(next ==
          "menu:pick_racer>menu:pick_racer2>race:RACEC0R1:respawn=3000:racer=0:"
          "circuit=c0:players=2:racer2=1:laps=5");
}
TEST_CASE("single race setup keeps the rules' opponent count") {
    RaceSetupMenu menu(Tracks());

    const std::optional<std::string> next =
        Press(menu, {MenuAction::Down, MenuAction::Confirm},
              {.respawn = {.timeout = Milliseconds{3000}}, .opponents = 1});

    CHECK(next == "menu:pick_racer>race:RACEC0R1:respawn=3000:racer=0:circuit=c0:opponents=1");
}

std::vector<CourseChoice> Circuits() {
    return {{.circuit = "c0", .racers = {{.code = "a"}, {.code = "b"}}},
            {.circuit = "c1", .circuitIndex = 1, .racers = {{.code = "c"}}, .isLocked = true}};
}

TEST_CASE("time trial setup builds the time trial scene name from its choices") {
    RaceSetupMenu menu(Tracks());

    const std::optional<std::string> next =
        Press(menu, {MenuAction::Down, MenuAction::Confirm},
              {.respawn = {.timeout = Milliseconds{3000}}, .isTimeTrial = true});

    CHECK(next == "menu:pick_racer>trial:RACEC0R1:respawn=3000:racer=0");
}

TEST_CASE("circuit setup picks the racer, then plays the circuit's intro and its first race") {
    RaceSetupMenu menu(Circuits());

    const std::optional<std::string> next = Press(menu, {MenuAction::Down, MenuAction::Confirm});

    CHECK(next ==
          "menu:pick_racer>screen:circuit1>circuit:c0:respawn=3000:racer=0:race=0:"
          "points=0,0,0,0,0,0");
}

TEST_CASE("the Rocket Racer circuit's intro names the grid's driver for want of a racer") {
    RaceSetupMenu menu({{.circuit = "c6", .racers = {{.code = "RR"}, {.code = "JT"}}}});

    const std::optional<std::string> next = Press(menu, {MenuAction::Down, MenuAction::Confirm});

    CHECK(next ==
          "menu:pick_racer>screen:circuit7:driver=RR>circuit:c6:respawn=3000:racer=0:"
          "race=0:points=0,0,0,0,0,0");
}

TEST_CASE("a circuit without an intro starts its first race") {
    RaceSetupMenu menu({{.circuit = "c9", .racers = {{.code = "a"}}}});

    const std::optional<std::string> next = Press(menu, {MenuAction::Down, MenuAction::Confirm});

    CHECK(next == "menu:pick_racer>circuit:c9:respawn=3000:racer=0:race=0:points=0,0,0,0,0,0");
}

TEST_CASE("a locked circuit can't start") {
    RaceSetupMenu menu(Circuits());

    const std::optional<std::string> next =
        Press(menu, {MenuAction::Right, MenuAction::Down, MenuAction::Confirm});

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

void AddModTrack(assets::Vfs& vfs, const std::filesystem::path& root) {
    const std::filesystem::path folder = root / "GAMEDATA" / "MODTRACK";
    std::filesystem::create_directories(folder);
    for (const std::string_view file : {"STARTPOS.SPB", "RACE.CPB"}) {
        std::ofstream(folder / file).put('\0');
    }
    Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(root);
    REQUIRE(layer.has_value());
    vfs.Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
}

TEST_CASE("single race lists tracks only mods add last, with the first circuit's grid") {
    std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder mod;
    AddModTrack(*vfs, mod.Path());

    const Result<std::vector<CourseChoice>> tracks = LoadTrackChoices(*vfs);

    REQUIRE(tracks.has_value());
    CHECK(tracks->size() == 14);
    CHECK(tracks->back().track + tracks->back().circuit == "MODTRACK");
    CHECK(tracks->back().racers.at(1).code == "CR");
}

TEST_CASE("time trials offer the races of the first three circuits, locked with them") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<std::vector<CourseChoice>> tracks = LoadTrialChoices(*vfs, {"c2"});

    REQUIRE(tracks.has_value());
    CHECK(tracks->size() == 12);
    CHECK(std::ranges::all_of(*tracks, [](const CourseChoice& track) {
        return track.isLocked == (track.circuit == "c1") && track.circuitIndex < 3;
    }));
}

TEST_CASE("a time trial race keeps its race-list index") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<std::vector<CourseChoice>> tracks = LoadTrialChoices(*vfs, {});

    REQUIRE(tracks.has_value());
    const auto racec0r0 = std::ranges::find(*tracks, "RACEC0R0", &CourseChoice::track);
    REQUIRE(racec0r0 != tracks->end());
    CHECK(racec0r0->race == 5);
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
