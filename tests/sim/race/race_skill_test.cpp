#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "../ai/straight_route.hpp"
#include "../physics/plane_world.hpp"
#include "checkpoint_ring.hpp"
#include "sim/ai/ai_skill.hpp"
#include "sim/ai/route_driver.hpp"
#include "sim/race/race_session.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
constexpr PowerupHabits kNeverUses{.useChances = {}};
constexpr int kMostTicks = 2000;
constexpr int kTicksTo15Seconds = 1250;

struct Fixture {
    CheckpointGraph graph = test::Ring();
    test::PlaneWorld world = test::FlatGround();
    std::array<RacerStart, 2> grid{RacerStart{.position = {.x = -20.0F, .z = 0.2F}},
                                   RacerStart{.position = {.x = -20.0F, .y = -10.0F, .z = 0.2F}}};
    GridRoutes routes{{test::StraightRoute({.x = -21.0F, .z = 0.2F})},
                      {test::StraightRoute({.x = -21.0F, .y = -10.0F, .z = 0.2F})}};

    [[nodiscard]] RaceSession Start(const AiSkill& skill) const {
        const RaceSetup setup{.racers = {{.chassis = kChassis, .isPlayer = true},
                                         {.chassis = kChassis, .habits = kNeverUses}},
                              .aiDriving = AiDriving::Physics,
                              .aiSkill = skill};
        Result<RaceSession> session = RaceSession::Create(setup, {.checkpoints = &graph,
                                                                  .grid = grid,
                                                                  .world = &world,
                                                                  .lowestTrackZ = -100.0F,
                                                                  .routes = &routes});
        REQUIRE(session.has_value());
        return std::move(*session);
    }
};

void Run(RaceSession& session, int ticks) {
    const std::vector<RacerControls> idle(session.Racers().size());
    for (int tick = 0; tick < ticks; ++tick) {
        session.Tick(kTick, idle);
    }
}

void RunUntilRacing(RaceSession& session) {
    for (int tick = 0; tick < kMostTicks && session.Clock().Phase() != RacePhase::Racing; ++tick) {
        Run(session, 1);
    }
}

float SkillShare(const RaceSession& session, std::size_t racer) {
    const std::optional<RouteDriver>& driver = session.DriverOf(racer);
    const std::optional<SkillProfile> skill = driver ? driver->Skill() : std::nullopt;
    REQUIRE(skill.has_value());
    return skill.value_or(SkillProfile{}).speedShare;
}

TEST_CASE("a skilled physics racer drives at the level its route's pace gives it") {
    Fixture fixture;
    fixture.routes[1].front().pace = assets::RoutePace::Slow;

    const RaceSession session = fixture.Start({.difficulty = 4});

    CHECK(SkillShare(session, 1) == ProfileFor(2).speedShare);
}

TEST_CASE("without catch-up a skilled racer keeps its level") {
    const Fixture fixture;
    RaceSession session = fixture.Start({});
    RunUntilRacing(session);

    Run(session, kTicksTo15Seconds + 10);

    CHECK(SkillShare(session, 1) == ProfileFor(kChampionLevel).speedShare);
}

TEST_CASE("fair catch-up never lifts a skilled racer ahead of an idle player") {
    const Fixture fixture;
    RaceSession session = fixture.Start({.catchesUp = true});
    RunUntilRacing(session);

    Run(session, kTicksTo15Seconds - 10);
    CHECK(SkillShare(session, 1) == ProfileFor(kChampionLevel).speedShare);
    Run(session, 20);

    CHECK(SkillShare(session, 1) <= ProfileFor(kChampionLevel).speedShare);
}

}
}
