#include <doctest/doctest.h>

#include <array>

#include "../physics/plane_world.hpp"
#include "checkpoint_ring.hpp"
#include "sim/race/race_session.hpp"

namespace rr::sim {
namespace {

constexpr ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};

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

}
}
