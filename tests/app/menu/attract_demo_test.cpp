#include "app/menu/attract_demo.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "app/race_scene_name.hpp"
#include "assets/archive_survey.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

TEST_CASE("a race scene name marks a demo") {
    const RaceSceneSetup demo{.track = "RACEC0R1", .circuit = "c0", .isDemo = true};

    CHECK(RaceSceneName(demo) == "race:RACEC0R1:respawn=3000:racer=0:circuit=c0:demo");
    CHECK(ParseRaceScene(RaceSceneName(demo)) == demo);
    CHECK_FALSE(ParseRaceScene("race:RACEC0R1:respawn=3000:demo=yes").has_value());
}

TEST_CASE("any bound action or menu press ends a demo") {
    sim::InputFrame input;
    CHECK_FALSE(input.WasAnythingPressed());
    input.players[1].pressed.set(sim::PlayerInput::Index(sim::Action::LookBack));
    CHECK(input.WasAnythingPressed());
    input = {};
    input.menuPressed.set(sim::InputFrame::Index(sim::MenuAction::Back));
    CHECK(input.WasAnythingPressed());
}

TEST_CASE("the attract timer runs out after a minute and waits for a dialog") {
    AttractTimer timer;
    CHECK_FALSE(timer.Tick(kAttractTimeout - Milliseconds{1}, true));
    CHECK_FALSE(timer.Tick(Milliseconds{1}, false));
    CHECK_FALSE(timer.Tick(Milliseconds{100}, false));
    CHECK(timer.Tick(Milliseconds{0}, true));
}

TEST_CASE("the attract demo races one of c0's four races") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const Result<assets::RaceCatalog> races = assets::RaceCatalog::Load(*vfs);
    REQUIRE(races.has_value());
    constexpr std::array<std::string_view, 4> kTracks{"RACEC0R1", "RACEC1R0", "RACEC0R3",
                                                      "RACEC0R2"};
    for (std::uint32_t roll = 0; roll < 8; ++roll) {
        CAPTURE(roll);
        const RaceSceneSetup expected{.track = std::string(kTracks.at(roll % kTracks.size())),
                                      .circuit = "c0",
                                      .isDemo = true};
        CHECK(AttractDemoScene(*races, roll).and_then([](const std::string& scene) {
            return ParseRaceScene(scene);
        }) == expected);
    }
}

}
