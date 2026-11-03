#include "app/race_scene_name.hpp"

#include <doctest/doctest.h>

#include <array>
#include <string_view>

#include "app/race_settings.hpp"
#include "settings/command_line.hpp"
#include "settings/registry.hpp"

namespace rr::app {
namespace {

TEST_CASE("a race scene name carries its track, respawn rule and racer") {
    const RaceSceneSetup timed{
        .track = "RACEC0R0", .respawn = {.timeout = Milliseconds{5000}}, .racer = 2};
    const RaceSceneSetup off{.track = "RACEC1R2", .respawn = {.isEnabled = false}};

    CHECK(RaceSceneName(timed) == "race:RACEC0R0:respawn=5000:racer=2");
    CHECK(RaceSceneName(off) == "race:RACEC1R2:respawn=off:racer=0");
    CHECK(ParseRaceScene("race:RACEC1R2:respawn=off") == off);
    CHECK(ParseRaceScene(RaceSceneName(timed)) == timed);
    CHECK(ParseRaceScene(RaceSceneName(off)) == off);
    CHECK_FALSE(IsRaceScene("RACEC0R0"));
}

TEST_CASE("a time trial scene name races the player alone over 3 laps") {
    const RaceSceneSetup trial{
        .track = "RACEC0R0", .respawn = {}, .racer = 2, .opponents = 0, .isTimeTrial = true};

    CHECK(RaceSceneName(trial) == "trial:RACEC0R0:respawn=3000:racer=2");
    CHECK(ParseTrialScene(RaceSceneName(trial)) == trial);
    CHECK(IsTrialScene(RaceSceneName(trial)));
    CHECK_FALSE(IsRaceScene(RaceSceneName(trial)));
    CHECK(ParseTrialScene("trial:racec0r0:respawn=3000:racer=2") == trial);
    CHECK_FALSE(ParseTrialScene("trial:RACEC0R0:respawn=3000:laps=5"));
}

TEST_CASE("a race scene name records physics-driven computer racers") {
    const RaceSceneSetup physics{
        .track = "RACEC0R0", .respawn = {}, .aiDriving = sim::AiDriving::Physics};

    CHECK(RaceSceneName(physics) == "race:RACEC0R0:respawn=3000:racer=0:ai=physics");
    CHECK(ParseRaceScene(RaceSceneName(physics)) == physics);
    CHECK(ParseRaceScene("race:RACEC0R0:respawn=3000:ai=route") ==
          RaceSceneSetup{.track = "RACEC0R0", .respawn = {}});
}

TEST_CASE("a race scene name records a circuit other than the track's home circuit") {
    const RaceSceneSetup circuit{.track = "RACEC1R0", .circuit = "c0", .respawn = {}};

    CHECK(RaceSceneName(circuit) == "race:RACEC1R0:respawn=3000:racer=0:circuit=c0");
    CHECK(ParseRaceScene(RaceSceneName(circuit)) == circuit);
    CHECK(ParseRaceScene("race:RACEC1R0:respawn=3000").value_or(circuit).circuit.empty());
}

TEST_CASE("a race scene name marks a mirrored race") {
    const RaceSceneSetup mirrored{.track = "RACEC0R0", .respawn = {}, .isMirrored = true};

    CHECK(RaceSceneName(mirrored) == "race:RACEC0R0:respawn=3000:racer=0:mirrored");
    CHECK(ParseRaceScene(RaceSceneName(mirrored)) == mirrored);
    CHECK_FALSE(ParseRaceScene("race:RACEC0R0:respawn=3000:mirrored=no").has_value());
}

TEST_CASE("a race scene name marks a test drive") {
    const RaceSceneSetup drive{.track = "TEST", .isTestDrive = true, .opponents = 0};

    CHECK(RaceSceneName(drive) == "race:TEST:respawn=3000:racer=0:testdrive:opponents=0");
    CHECK(ParseRaceScene(RaceSceneName(drive)) == drive);
    CHECK_FALSE(ParseRaceScene("race:TEST:respawn=3000:testdrive=1").has_value());
}

TEST_CASE("a race scene name records a two-player race") {
    const RaceSceneSetup versus{.track = "RACEC0R0", .players = 2};

    CHECK(RaceSceneName(versus) == "race:RACEC0R0:respawn=3000:racer=0:players=2:racer2=1");
    CHECK(ParseRaceScene(RaceSceneName(versus)) == versus);
    CHECK(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0")->players == 1);
    CHECK_FALSE(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0:players=3"));
    CHECK_FALSE(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0:players=0"));
    CHECK_FALSE(
        ParseCircuitScene("circuit:c0:respawn=3000:racer=0:race=0:"
                          "points=0,0,0,0,0,0:players=2"));
}

TEST_CASE("a versus race scene name records player 2's racer and the lap count") {
    const RaceSceneSetup versus{
        .track = "RACEC0R0", .racer = 3, .players = 2, .secondRacer = 3, .lapCount = 5};

    CHECK(RaceSceneName(versus) == "race:RACEC0R0:respawn=3000:racer=3:players=2:racer2=3:laps=5");
    CHECK(ParseRaceScene(RaceSceneName(versus)) == versus);
    CHECK(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0")->lapCount == 3);
    CHECK_FALSE(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0:laps=0"));
    CHECK_FALSE(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0:laps=10"));
    CHECK_FALSE(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0:players=2:racer2=6"));
    CHECK_FALSE(
        ParseCircuitScene("circuit:c0:respawn=3000:racer=0:race=0:points=0,0,0,0,0,0:laps=5"));
}

TEST_CASE("every race scene name carries the player's own racer, escaped") {
    const RaceSceneSetup race{.track = "RACEC0R0", .player = {.name = "MR:X>50% @2"}};
    const RaceSceneSetup trial{.track = "RACEC0R0", .isTimeTrial = true, .player = {.name = "ME"}};
    const RacerRef copy{.name = "ME", .hash = 0xABC};
    const CircuitSceneSetup circuit{.race = {.circuit = "c0", .player = copy},
                                    .standing = {.points = {}}};

    CHECK(RaceSceneName(race) == "race:RACEC0R0:respawn=3000:racer=0:player=MR%3AX%3E50%25%20%402");
    CHECK(ParseRaceScene(RaceSceneName(race)) == race);
    CHECK(ParseTrialScene(RaceSceneName(trial))->player == trial.player);
    CHECK(CircuitSceneName(circuit).contains(":player=ME@0000000000000abc:"));
    CHECK(ParseCircuitScene(CircuitSceneName(circuit))->race.player == copy);
    CHECK_FALSE(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0:player=ME@"));
    CHECK_FALSE(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0:player=ME@xyz"));
    CHECK_FALSE(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0:player="));
    CHECK_FALSE(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0:player=A%4"));
    CHECK_FALSE(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0:player=A%G0"));
}

TEST_CASE("the racer pick names its racer in every race of a scene chain") {
    const RacerRef me{.name = "ME", .hash = 1};

    CHECK(WithPlayer("screen:circuit1>circuit:c0:respawn=off:racer=0", me) ==
          "screen:circuit1>circuit:c0:respawn=off:racer=0:player=ME@0000000000000001:race=0:"
          "points=0,0,0,0,0,0");
    CHECK(WithPlayer("trial:RACEC0R0:respawn=off:racer=0", {.name = "ME"}) ==
          "trial:RACEC0R0:respawn=off:racer=0:player=ME");
    CHECK(ScenePlayer(WithPlayer("race:RACEC0R0:respawn=off:racer=0", me)) == me);
    CHECK(WithPlayer("menu:main", me) == "menu:main");
    CHECK(WithPlayer("race:RACEC0R0:respawn=off:racer=0:player=YOU", me) ==
          "race:RACEC0R0:respawn=off:racer=0:player=ME@0000000000000001");
}

TEST_CASE("a versus race scene name carries player 2's own racer") {
    const RaceSceneSetup versus{
        .track = "RACEC0R0", .players = 2, .secondPlayer = {.name = "YOU", .hash = 2}};
    const std::string chain = "menu:pick_racer2>race:RACEC0R0:respawn=3000:racer=0:players=2";

    CHECK(RaceSceneName(versus) ==
          "race:RACEC0R0:respawn=3000:racer=0:players=2:racer2=1:player2=YOU@0000000000000002");
    CHECK(ParseRaceScene(RaceSceneName(versus)) == versus);
    CHECK(SceneSecondPlayer(RaceSceneName(versus)) == versus.secondPlayer);
    CHECK(WithSecondPlayer(chain, {.name = "ME"}) ==
          "menu:pick_racer2>race:RACEC0R0:respawn=3000:racer=0:players=2:racer2=1:player2=ME");
    CHECK_FALSE(ParseCircuitScene("circuit:c0:respawn=3000:racer=0:player2=ME"));
}

TEST_CASE("a single race scene name records fewer opponents than a full grid") {
    const RaceSceneSetup race{.track = "RACEC0R0", .opponents = 2};

    CHECK(RaceSceneName(race) == "race:RACEC0R0:respawn=3000:racer=0:opponents=2");
    CHECK(ParseRaceScene(RaceSceneName(race)) == race);
    CHECK(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0")->opponents == 5);
    CHECK(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0:opponents=0")->opponents == 0);
    CHECK_FALSE(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0:opponents=6"));
}

TEST_CASE("a race scene name carries its cheats, which circuits and time trials refuse") {
    const RaceSceneSetup race{.track = "RACEC0R0", .cheats = *sim::ParseCheatPlates("MXPMX,NWHLS")};

    CHECK(RaceSceneName(race) == "race:RACEC0R0:respawn=3000:racer=0:cheats=MXPMX,NWHLS");
    CHECK(ParseRaceScene(RaceSceneName(race)) == race);
    CHECK_FALSE(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0:cheats="));
    CHECK_FALSE(ParseRaceScene("race:RACEC0R0:respawn=3000:racer=0:cheats=MXPMX,NOPE"));
    CHECK_FALSE(ParseTrialScene("trial:RACEC0R0:respawn=3000:racer=0:cheats=MXPMX"));
    CHECK_FALSE(ParseCircuitScene("circuit:c0:respawn=off:cheats=MXPMX"));
}

TEST_CASE("the mirror cheat mirrors only the Rocket Racer circuit's race") {
    const sim::Cheats mirror = *sim::ParseCheatPlates("LNFRRRM");

    CHECK(LoadsMirrored({.track = "RACEC6R0", .circuit = "c6", .cheats = mirror}));
    CHECK_FALSE(LoadsMirrored({.track = "RACEC6R0", .circuit = "c6"}));
    CHECK_FALSE(LoadsMirrored({.track = "RACEC0R0", .circuit = "c0", .cheats = mirror}));
    CHECK(LoadsMirrored({.track = "RACEC0R0", .isMirrored = true}));
}

TEST_CASE("a lone player races the opponents the grid holds, and two players race alone") {
    constexpr std::size_t kGridSize = 6;

    CHECK(RacerCount({}, kGridSize) == 6);
    CHECK(RacerCount({.opponents = 2}, kGridSize) == 3);
    CHECK(RacerCount({.opponents = 0}, kGridSize) == 1);
    CHECK(RacerCount({}, 4) == 4);
    CHECK(RacerCount({.players = 2, .opponents = 3}, kGridSize) == 2);
}

TEST_CASE("a malformed race scene name is rejected") {
    for (const std::string_view scene :
         {"RACEC0R0", "race:RACEC0R0", "race::respawn=3000", "race:RACEC0R0:respawn=0",
          "race:RACEC0R0:respawn=3s", "race:RACEC0R0:respawn=",
          "race:RACEC0R0:respawn=3000:racer=6", "race:RACEC0R0:respawn=3000:racer=-1",
          "race:RACEC0R0:respawn=3000:difficulty=1", "race:RACEC0R0:respawn=3000:colour=1",
          "race:RACEC0R0:respawn=3000:ai=smart", "race:RACEC0R0:respawn=3000:circuit="}) {
        CAPTURE(scene);
        CHECK_FALSE(ParseRaceScene(scene).has_value());
    }
}

TEST_CASE("a circuit scene name carries the run's standing") {
    const CircuitSceneSetup setup{
        .race = {.circuit = "c2", .respawn = {.isEnabled = false}, .racer = 3},
        .standing = {.race = 2, .points = {30, 20, 10, 3, 2, 1}}};

    const std::string name = CircuitSceneName(setup);

    CHECK(name == "circuit:c2:respawn=off:racer=3:race=2:points=30,20,10,3,2,1");
    CHECK(IsCircuitScene(name));
    CHECK_FALSE(IsRaceScene(name));
    CHECK(ParseCircuitScene(name) == setup);
}

TEST_CASE("a circuit scene name needs a circuit, a race in range and six points") {
    CHECK_FALSE(ParseCircuitScene("circuit::respawn=off:race=0").has_value());
    CHECK_FALSE(ParseCircuitScene("circuit:c0:respawn=off:race=4").has_value());
    CHECK_FALSE(ParseCircuitScene("circuit:c0:respawn=off:points=1,2,3").has_value());
    CHECK_FALSE(ParseCircuitScene("circuit:c0:respawn=off:points=1,2,3,4,5,6,7").has_value());
    CHECK_FALSE(ParseCircuitScene("race:RACEC0R0:respawn=off:race=0").has_value());
    CHECK(ParseCircuitScene("circuit:c0:respawn=off").has_value());
}

TEST_CASE("respawn settings default on after 3 seconds and follow their values") {
    settings::Registry registry;
    DeclareRaceSettings(registry);
    const sim::RespawnRule defaults = RespawnRuleFrom(registry);
    CHECK(defaults.isEnabled);
    CHECK(defaults.timeout == Milliseconds{3000});

    const std::array<settings::SettingOverride, 2> overrides{
        settings::SettingOverride{.key = "race.respawn", .value = "false"},
        settings::SettingOverride{.key = "race.respawn_seconds", .value = "5"}};
    REQUIRE(settings::ApplyOverrides(registry, overrides).has_value());

    const sim::RespawnRule changed = RespawnRuleFrom(registry);
    CHECK_FALSE(changed.isEnabled);
    CHECK(changed.timeout == Milliseconds{5000});
}

TEST_CASE("computer racers play back their routes unless physics driving is on") {
    settings::Registry registry;
    DeclareRaceSettings(registry);
    CHECK(AiDrivingFrom(registry) == sim::AiDriving::Route);

    registry.Set(kPhysicsAi, true);

    CHECK(AiDrivingFrom(registry) == sim::AiDriving::Physics);
}

}
}
