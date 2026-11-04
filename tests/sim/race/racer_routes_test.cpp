#include "sim/race/racer_routes.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>

#include "../ai/straight_route.hpp"
#include "../physics/plane_world.hpp"
#include "checkpoint_ring.hpp"
#include "sim/race/race_session.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
constexpr float kSide = 200.0F;
constexpr int kTicksTo15Seconds = 1250;

CheckpointGraph Square() {
    using test::GateAt;
    using test::kNo;
    return CheckpointGraph::Create(
               {.checkpoints = {GateAt(0.0F, {1, kNo, kNo, kNo}), GateAt(kSide, {2, kNo, kNo, kNo}),
                                GateAt(kSide, {3, kNo, kNo, kNo}, kSide),
                                GateAt(0.0F, {0, kNo, kNo, kNo}, kSide)}})
        .value();
}

struct Fixture {
    CheckpointGraph graph = Square();
    test::PlaneWorld world = test::FlatGround();
    std::array<RacerStart, 2> grid{RacerStart{.position = {.x = -40.0F, .z = 0.2F}},
                                   RacerStart{.position = {.x = -40.0F, .y = -10.0F, .z = 0.2F}}};
    GridRoutes routes{{test::StraightRoute({.x = -41.0F, .z = 0.2F})},
                      {test::StraightRoute({.x = -41.0F, .y = -10.0F, .z = 0.2F})}};

    [[nodiscard]] RaceCourse Course() const {
        return {.checkpoints = &graph,
                .grid = grid,
                .world = &world,
                .lowestTrackZ = -100.0F,
                .routes = &routes};
    }
};

RaceSetup TwoRacers(AiDriving driving, AiCraft craft = {}) {
    return {.racers = {{.chassis = kChassis, .isPlayer = true}, {.chassis = kChassis}},
            .aiDriving = driving,
            .aiCraft = craft};
}

TEST_CASE("computer racers drive their slot's recordings unless they ask for gate paths") {
    const Fixture fixture;

    const RacerRoutes plain(TwoRacers(AiDriving::Physics), fixture.Course());
    const RacerRoutes lined(TwoRacers(AiDriving::Physics, {.racingLine = true}), fixture.Course());
    const RacerRoutes gated(TwoRacers(AiDriving::Physics, {.gatePaths = true}), fixture.Course());
    const RacerRoutes routed(TwoRacers(AiDriving::Route, {.gatePaths = true}), fixture.Course());

    CHECK_FALSE(plain.IsGatePath(1));
    CHECK(plain.LinesOf(1).empty());
    CHECK(lined.LinesOf(1).size() == 1);
    CHECK_FALSE(routed.IsGatePath(1));
    CHECK(gated.IsGatePath(1));
    CHECK(gated.Of(1).size() == 1);
    CHECK(gated.RecordedOf(1).size() == 1);
    CHECK(gated.LinesOf(1).size() == 1);
    CHECK_FALSE(gated.IsGatePath(0));
}

TEST_CASE("computer racers on a track without recordings drive gate paths") {
    Fixture fixture;
    fixture.routes.clear();

    const RacerRoutes routes(TwoRacers(AiDriving::Route), fixture.Course());

    CHECK(routes.IsGatePath(1));
    CHECK(routes.RecordedOf(1).empty());
    CHECK(routes.LinesOf(1).size() == 1);
}

TEST_CASE("a slot without recordings on a recorded track keeps no route unless asked") {
    Fixture fixture;
    fixture.routes[1].clear();

    const RacerRoutes routed(TwoRacers(AiDriving::Route), fixture.Course());
    const RacerRoutes gated(TwoRacers(AiDriving::Physics, {.gatePaths = true}), fixture.Course());

    CHECK(routed.Of(1).empty());
    CHECK(gated.IsGatePath(1));
}

TEST_CASE("a track whose checkpoints don't loop gives no gate path") {
    Fixture fixture;
    fixture.routes[1].clear();
    fixture.graph = CheckpointGraph::Create({}).value();

    const RacerRoutes routes(TwoRacers(AiDriving::Physics, {.gatePaths = true}), fixture.Course());

    CHECK_FALSE(routes.IsGatePath(1));
    CHECK(routes.Of(1).empty());
}

TEST_CASE("a race session drives a computer racer without recordings round its gate path") {
    Fixture fixture;
    fixture.routes.clear();
    Result<RaceSession> session =
        RaceSession::Create(TwoRacers(AiDriving::Route), fixture.Course());
    REQUIRE(session.has_value());
    const float startX = session->Racers()[1].Body().motion.position.x;

    const std::array<RacerControls, 2> controls{};
    for (int tick = 0; tick < kTicksTo15Seconds; ++tick) {
        session->Tick(kTick, controls);
    }

    CHECK(session->DriverOf(1).has_value());
    CHECK_FALSE(session->Racers()[1].Route().has_value());
    CHECK(session->Racers()[1].Body().motion.position.x > startX + 20.0F);
}

}
}
