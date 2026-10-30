#include "sim/race/race_session.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "../ai/straight_route.hpp"
#include "../physics/plane_world.hpp"
#include "checkpoint_ring.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
constexpr RacerControls kThrottle{.isThrottleHeld = true};
constexpr float kHighUp = 300.0F;
constexpr Vector3 kRouteStart{.x = -21.0F, .y = -10.0F, .z = 0.2F};
constexpr int kTicksTo15Seconds = 1250;

struct Fixture {
    CheckpointGraph graph = test::Ring();
    test::PlaneWorld world = test::FlatGround();
    std::array<RacerStart, 2> grid{RacerStart{.position = {.x = -20.0F, .z = 0.2F}},
                                   RacerStart{.position = {.x = -20.0F, .y = -10.0F, .z = 0.2F}}};
    GridRoutes routes{{test::StraightRoute({.x = -21.0F, .z = 0.2F})},
                      {test::StraightRoute(kRouteStart)}};

    [[nodiscard]] RaceSession Start(const RaceSetup& setup) const {
        Result<RaceSession> session = RaceSession::Create(setup, {.checkpoints = &graph,
                                                                  .grid = grid,
                                                                  .world = &world,
                                                                  .lowestTrackZ = -100.0F,
                                                                  .routes = &routes});
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

TEST_CASE("a computer racer drives a route from its slot once racing starts") {
    const Fixture fixture;
    RaceSession session = fixture.Start(TwoRacers());

    RunUntilRacing(session);
    CHECK(session.Racers()[1].Body().motion.position == kRouteStart);
    CHECK_FALSE(session.Racers()[0].Route().has_value());

    Run(session, 300);

    CHECK(session.Racers()[1].Body().motion.position.x == doctest::Approx(15.0F));
    CHECK(session.Progress().OfRacer(1).laps.LastCheckpoint() == 1);
}

TEST_CASE("the session's seeded random picks each computer racer's route") {
    constexpr std::uint64_t kSeed = 7;
    Fixture fixture;
    fixture.routes[1] = {test::StraightRoute({.y = 1.0F}), test::StraightRoute({.y = 2.0F}),
                         test::StraightRoute({.y = 3.0F})};
    RaceSetup setup = TwoRacers();
    setup.seed = kSeed;

    const RaceSession session = fixture.Start(setup);

    Random random(kSeed);
    const auto expected = static_cast<float>(random.Below(3) + 1);
    CHECK(session.Racers()[1].Body().motion.position.y == expected);
}

TEST_CASE("after 15 s the rubber band slows a computer racer leading the player") {
    const Fixture fixture;
    RaceSession session = fixture.Start(TwoRacers());
    const auto speed = [&] {
        return session.Racers()[1]
            .Route()
            .transform([](const AiRacer& route) { return route.PlaybackSpeed(); })
            .value_or(0.0F);
    };

    Run(session, kTicksTo15Seconds, {});
    CHECK(speed() == 1.0F);

    Run(session, 10, {});
    CHECK(speed() == doctest::Approx(0.95F));
}

TEST_CASE("a computer racer on a route never respawns") {
    Fixture fixture;
    fixture.routes[1] = {test::StraightRoute({.y = -10.0F, .z = -kHighUp})};
    RaceSession session = fixture.Start(TwoRacers({.timeout = Milliseconds{1000}}));

    Run(session, 92);

    CHECK(session.Racers()[1].Body().motion.position.z == -kHighUp);
}

}
}
