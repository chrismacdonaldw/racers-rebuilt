#include "sim/race/race_session.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <optional>
#include <vector>

#include "../physics/plane_world.hpp"
#include "checkpoint_ring.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
constexpr RacerControls kThrottle{.isThrottleHeld = true};
constexpr float kHighUp = 300.0F;

struct Fixture {
    CheckpointGraph graph = test::Ring();
    test::PlaneWorld world = test::FlatGround();
    std::array<RacerStart, 2> grid{RacerStart{.position = {.x = -20.0F, .z = 0.2F}},
                                   RacerStart{.position = {.x = -20.0F, .y = -10.0F, .z = 0.2F}}};

    [[nodiscard]] RaceSession Start(const RaceSetup& setup) const {
        Result<RaceSession> session = RaceSession::Create(
            setup, {.checkpoints = &graph, .grid = grid, .world = &world, .lowestTrackZ = -100.0F});
        REQUIRE(session.has_value());
        return std::move(*session);
    }
};

RaceSetup TwoRacers(RespawnRule respawn = {}) {
    return {.racers = {{.chassis = kChassis, .isPlayer = true}, {.chassis = kChassis}},
            .respawn = respawn};
}

void Run(RaceSession& session, int ticks, const RacerControls& controls = kThrottle) {
    const std::vector<RacerControls> all(session.Racers().size(), controls);
    for (int tick = 0; tick < ticks; ++tick) {
        session.Tick(kTick, all);
    }
}

void RunUntilRacing(RaceSession& session) {
    while (session.Clock().Phase() != RacePhase::Racing) {
        Run(session, 1);
    }
}

TEST_CASE("racers hold their grid slots until racing starts, then drive through the gates") {
    const Fixture fixture;
    RaceSession session = fixture.Start(TwoRacers());

    RunUntilRacing(session);
    CHECK(std::abs(session.Racers()[0].Body().motion.position.x + 20.0F) < 0.5F);

    Run(session, 250);

    CHECK(session.Progress().OfRacer(0).laps.LastCheckpoint() == 3);
    CHECK(session.Progress().OfRacer(0).lapTimes[0] == Milliseconds{3000});
}

TEST_CASE("throttle right after go gives a strong start boost") {
    const Fixture fixture;
    RaceSession session = fixture.Start(TwoRacers());
    while (session.Clock().Phase() != RacePhase::Racing) {
        Run(session, 1, {});
    }

    Run(session, 1);

    const auto level = [](const Turbo& turbo) { return turbo.level; };
    CHECK(session.Racers()[0].ActiveTurbo().transform(level) == TurboLevel::Strong);
}

TEST_CASE("a racer off the ground respawns on its grid slot after the timeout") {
    Fixture fixture;
    fixture.grid[0].position.z = kHighUp;
    RaceSession session = fixture.Start(TwoRacers({.timeout = Milliseconds{1000}}));

    Run(session, 92);

    const Vector3& position = session.Racers()[0].Body().motion.position;
    CHECK(position.x == doctest::Approx(-20.0F));
    CHECK(position.z < 1.0F);
}

TEST_CASE("with respawn off, a falling racer keeps falling") {
    Fixture fixture;
    fixture.grid[0].position.z = kHighUp;
    RaceSession session = fixture.Start(TwoRacers({.isEnabled = false}));

    Run(session, 92);

    CHECK(session.Racers()[0].Body().motion.position.z > 100.0F);
}

TEST_CASE("a race needs a grid slot for every racer") {
    const Fixture fixture;
    const CheckpointGraph& graph = fixture.graph;
    RaceSetup setup = TwoRacers();
    setup.racers.emplace_back();

    CHECK_FALSE(RaceSession::Create(
                    setup, {.checkpoints = &graph, .grid = fixture.grid, .world = &fixture.world})
                    .has_value());
}

}
}
