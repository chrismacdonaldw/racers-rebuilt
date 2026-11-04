#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "../physics/plane_world.hpp"
#include "checkpoint_ring.hpp"
#include "sim/race/race_session.hpp"

namespace rr::sim {
namespace {

constexpr ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
constexpr Milliseconds kTick{12};

struct Course {
    CheckpointGraph graph = test::Ring();
    test::PlaneWorld world = test::FlatGround();
    std::array<RacerStart, 3> grid{RacerStart{.position = {.x = -20.0F, .z = 0.2F}},
                                   RacerStart{.position = {.x = -20.0F, .y = -10.0F, .z = 0.2F}},
                                   RacerStart{.position = {.x = -20.0F, .y = -20.0F, .z = 0.2F}}};

    [[nodiscard]] RaceCourse View() const {
        return {.checkpoints = &graph, .grid = grid, .world = &world, .lowestTrackZ = -100.0F};
    }
};

TEST_CASE("racers start from the setup's first grid slot") {
    const Course course;
    const RaceSetup setup{.racers = {{.chassis = kChassis, .isPlayer = true}}, .firstSlot = 2};

    const Result<RaceSession> session = RaceSession::Create(setup, course.View());

    REQUIRE(session.has_value());
    CHECK(session->Racers()[0].Pose().position.y == doctest::Approx(-20.0F));
}

TEST_CASE("racers that run off the end of the grid from the first slot fail") {
    const Course course;
    const RaceSetup setup{.racers = {{.chassis = kChassis}, {.chassis = kChassis}}, .firstSlot = 2};

    CHECK_FALSE(RaceSession::Create(setup, course.View()).has_value());
}

std::vector<std::uint8_t> EngineStartsBeforeCountdown(RaceSession& session) {
    const std::vector<RacerControls> idle(session.Racers().size());
    std::vector<std::uint8_t> started;
    while (session.Clock().Phase() == RacePhase::Intro) {
        session.Tick(kTick, idle);
        for (const SoundEvent& sound : session.Sounds()) {
            if (sound.sound == RaceSound::EngineStart) {
                started.push_back(sound.racer);
            }
        }
    }
    return started;
}

TEST_CASE("a demo's first racer starts its engine like a player") {
    const Course course;
    const RaceSetup setup{.racers = {{.chassis = kChassis}, {.chassis = kChassis}}, .isDemo = true};
    Result<RaceSession> session = RaceSession::Create(setup, course.View());
    REQUIRE(session.has_value());

    CHECK(EngineStartsBeforeCountdown(*session) == std::vector<std::uint8_t>{0});
    CHECK(session->HasCarSounds(0));
    CHECK_FALSE(session->HasCarSounds(1));
}

}
}
