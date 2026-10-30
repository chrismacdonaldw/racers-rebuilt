#include <doctest/doctest.h>

#include <array>
#include <string_view>

#include "app/race_scene.hpp"
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
