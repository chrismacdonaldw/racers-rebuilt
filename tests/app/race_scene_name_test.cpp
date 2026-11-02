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
