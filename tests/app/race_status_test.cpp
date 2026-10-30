#include "app/race_status.hpp"

#include <doctest/doctest.h>

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/race_scene.hpp"
#include "app/race_settings.hpp"
#include "settings/command_line.hpp"
#include "settings/registry.hpp"
#include "sim/physics/plane_world.hpp"
#include "sim/race/checkpoint_ring.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kTick{12};

struct Race {
    sim::CheckpointGraph graph = test::Ring();
    test::PlaneWorld world = test::FlatGround();
    std::array<sim::RacerStart, 2> grid{
        sim::RacerStart{.position = {.x = -20.0F, .z = 0.2F}},
        sim::RacerStart{.position = {.x = -20.0F, .y = -10.0F, .z = 0.2F}}};
    sim::RaceSession session = Start();
    std::string status;

    [[nodiscard]] sim::RaceSession Start() const {
        Result<sim::RaceSession> started = sim::RaceSession::Create(
            {.racers = {{.isPlayer = true}, {}}},
            {.checkpoints = &graph, .grid = grid, .world = &world, .lowestTrackZ = -100.0F});
        REQUIRE(started.has_value());
        return std::move(*started);
    }

    void Run(int ticks) {
        const std::vector<sim::RacerControls> controls(2);
        for (int tick = 0; tick < ticks; ++tick) {
            session.Tick(kTick, controls);
        }
    }

    void RunUntil(sim::RacePhase phase) {
        while (session.Clock().Phase() != phase) {
            Run(1);
        }
    }

    const std::string& Status() {
        FormatRaceStatus(session, 0, status);
        return status;
    }
};

TEST_CASE("the race status counts down before the start") {
    Race race;
    CHECK(race.Status() == "Intro");

    race.RunUntil(sim::RacePhase::Countdown);
    CHECK(race.Status() == "Countdown 3");

    race.Run(200);
    CHECK(race.Status() == "Countdown 1");
}

TEST_CASE("the race status shows the lap, place and race time while racing") {
    Race race;
    race.RunUntil(sim::RacePhase::Racing);

    race.Run(10);

    CHECK(race.Status() == "Lap 1/3  Place 1 of 2  0:00.120");
}

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

TEST_CASE("a malformed race scene name is rejected") {
    for (const std::string_view scene :
         {"RACEC0R0", "race:RACEC0R0", "race::respawn=3000", "race:RACEC0R0:respawn=0",
          "race:RACEC0R0:respawn=3s", "race:RACEC0R0:respawn=",
          "race:RACEC0R0:respawn=3000:racer=6", "race:RACEC0R0:respawn=3000:racer=-1",
          "race:RACEC0R0:respawn=3000:difficulty=1", "race:RACEC0R0:respawn=3000:colour=1"}) {
        CAPTURE(scene);
        CHECK_FALSE(ParseRaceScene(scene).has_value());
    }
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

}
}
