#include "app/menu/single_race_menu.hpp"

#include <doctest/doctest.h>

#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/menu_scenes.hpp"
#include "assets/archive_survey.hpp"

namespace rr::app {
namespace {

using sim::MenuAction;

std::vector<TrackChoice> Tracks() {
    return {{.track = "RACEC0R1",
             .circuit = "c0",
             .racers = {{.code = "a"}, {.code = "b"}, {.code = "c"}}},
            {.track = "RACEC0R0", .circuit = "c1", .racers = {{.code = "d"}, {.code = "e"}}}};
}

std::optional<std::string> Press(SingleRaceMenu& menu, std::initializer_list<MenuAction> actions) {
    std::optional<std::string> next;
    for (const MenuAction action : actions) {
        sim::InputFrame frame;
        frame.menuPressed.set(sim::InputFrame::Index(action));
        next = menu.Tick(frame, {.respawn = {.timeout = Milliseconds{3000}}});
    }
    return next;
}

TEST_CASE("single race setup builds the race scene name from its choices") {
    SingleRaceMenu menu(Tracks());

    const std::optional<std::string> next =
        Press(menu, {MenuAction::Right, MenuAction::Down, MenuAction::Right, MenuAction::Down,
                     MenuAction::Confirm});

    REQUIRE(next.has_value());
    CHECK(*next == "race:RACEC0R0:respawn=3000:racer=1:circuit=c1");
}

TEST_CASE("changing track keeps the racer within its grid") {
    SingleRaceMenu menu(Tracks());

    static_cast<void>(
        Press(menu, {MenuAction::Down, MenuAction::Left, MenuAction::Up, MenuAction::Right}));

    CHECK(menu.Track().track == "RACEC0R0");
    CHECK(menu.Racer() == 1);
}

TEST_CASE("back leaves for the main menu from any row") {
    SingleRaceMenu menu(Tracks());

    CHECK(Press(menu, {MenuAction::Up, MenuAction::Confirm}) == std::string(kMainMenuScene));
    CHECK(Press(menu, {MenuAction::Down, MenuAction::Back}) == std::string(kMainMenuScene));
}

TEST_CASE("confirming a choice moves on to the next row") {
    SingleRaceMenu menu(Tracks());

    CHECK_FALSE(Press(menu, {MenuAction::Confirm}).has_value());
    CHECK(menu.Focus() == SingleRaceRow::Racer);
}

void CheckRacerNamed(const RacerChoice& racer, const std::vector<std::u16string>& names) {
    CHECK_MESSAGE(racer.nameIndex >= 0, racer.code);
    CHECK_MESSAGE(static_cast<std::size_t>(racer.nameIndex) < names.size(), racer.code);
}

void CheckRacersNamed(const std::vector<TrackChoice>& tracks,
                      const std::vector<std::u16string>& names) {
    for (const TrackChoice& track : tracks) {
        CHECK_MESSAGE(track.racers.size() == 6, track.track);
        for (const RacerChoice& racer : track.racers) {
            CheckRacerNamed(racer, names);
        }
    }
}

void CheckTracksNamed(const std::vector<TrackChoice>& tracks,
                      const std::vector<std::u16string>& names) {
    for (const TrackChoice& track : tracks) {
        CHECK_MESSAGE(track.nameLine >= 0, track.track);
        CHECK_MESSAGE(static_cast<std::size_t>(track.nameLine) < names.size(), track.track);
    }
}

TEST_CASE("every unmirrored retail circuit race offers a full grid of named racers") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<std::vector<TrackChoice>> tracks = LoadTrackChoices(*vfs);

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

    const Result<std::vector<TrackChoice>> tracks = LoadTrackChoices(*vfs);

    REQUIRE(tracks.has_value());
    CHECK(tracks->front().track == "RACEC0R1");
    CHECK(tracks->front().circuit == "c0");
    CHECK(tracks->front().racers.at(1).code == "CR");
}

}
}
