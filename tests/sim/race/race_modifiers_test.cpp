#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "../ai/straight_route.hpp"
#include "../physics/plane_world.hpp"
#include "../powerup/fake_effect.hpp"
#include "checkpoint_ring.hpp"
#include "sim/physics/racer.hpp"
#include "sim/race/race_session.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
constexpr PowerupHabits kNeverUses{.useChances = {}};
constexpr RacerControls kUse{.isUsePressed = true};
constexpr int kMostTicks = 2000;

struct Fixture {
    CheckpointGraph graph = test::Ring();
    test::PlaneWorld world = test::FlatGround();
    std::array<RacerStart, 2> grid{RacerStart{.position = {.x = -20.0F, .z = 0.2F}},
                                   RacerStart{.position = {.x = -20.0F, .y = -10.0F, .z = 0.2F}}};
    GridRoutes routes{{test::StraightRoute({.x = -21.0F, .z = 0.2F})},
                      {test::StraightRoute({.x = -21.0F, .y = -10.0F, .z = 0.2F})}};
    assets::BrickLayout bricks{
        .coloured = {{.position = {.x = -20.0F, .z = 0.2F}, .colour = assets::BrickColour::Red}}};

    [[nodiscard]] RaceSession Start(const Modifiers& modifiers, std::uint64_t seed = 1) const {
        const RaceSetup setup{.racers = {{.chassis = kChassis, .isPlayer = true},
                                         {.chassis = kChassis, .habits = kNeverUses}},
                              .seed = seed,
                              .effects = test::FakeEffects(Milliseconds{60'000}),
                              .modifiers = modifiers};
        Result<RaceSession> session = RaceSession::Create(setup, {.checkpoints = &graph,
                                                                  .grid = grid,
                                                                  .world = &world,
                                                                  .lowestTrackZ = -100.0F,
                                                                  .routes = &routes,
                                                                  .bricks = &bricks});
        REQUIRE(session.has_value());
        return std::move(*session);
    }
};

void Run(RaceSession& session, int ticks, const RacerControls& controls = {}) {
    const std::vector<RacerControls> all(session.Racers().size(), controls);
    for (int tick = 0; tick < ticks; ++tick) {
        session.Tick(kTick, all);
    }
}

void RunUntilRacing(RaceSession& session) {
    for (int tick = 0; tick < kMostTicks && session.Clock().Phase() != RacePhase::Racing; ++tick) {
        Run(session, 1);
    }
}

Racer Dropped(float gravityScale) {
    Racer racer(kChassis, {}, {.position = {.z = 20.0F}});
    racer.ScaleGravity(gravityScale);
    return racer;
}

int TicksToLand(float gravityScale) {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer = Dropped(gravityScale);
    int ticks = 0;
    while (!IsGrounded(racer.Body().motion) && ticks < kMostTicks) {
        racer.Tick(kTick, {}, world);
        ++ticks;
    }
    return ticks;
}

}

TEST_CASE("quarter gravity quarters a dropped racer's first pull and slows its fall") {
    const test::PlaneWorld world = test::FlatGround();
    Racer full = Dropped(1.0F);
    Racer quarter = Dropped(0.25F);
    full.Tick(kTick, {}, world);
    quarter.Tick(kTick, {}, world);

    REQUIRE(full.Body().motion.velocity.z < 0.0F);
    CHECK(quarter.Body().motion.velocity.z ==
          doctest::Approx(0.25F * full.Body().motion.velocity.z));
    CHECK(TicksToLand(0.25F) > TicksToLand(1.0F) * 4 / 3);
}

TEST_CASE("hyperdrive scales every racer's top speed and acceleration") {
    const Fixture fixture;
    const RaceSession plain = fixture.Start({});
    const RaceSession fast = fixture.Start({.speedScale = 1.5F});

    for (std::size_t i = 0; i < plain.Racers().size(); ++i) {
        CHECK(fast.Racers()[i].Stats().topSpeed ==
              doctest::Approx(1.5F * plain.Racers()[i].Stats().topSpeed));
        CHECK(fast.Racers()[i].Stats().acceleration ==
              doctest::Approx(1.5F * plain.Racers()[i].Stats().acceleration));
        CHECK(fast.Racers()[i].Body().maxSpeed ==
              doctest::Approx(1.5F * plain.Racers()[i].Body().maxSpeed));
    }
}

TEST_CASE("only this power-up turns a held green into the chosen kind") {
    const Fixture fixture;
    RaceSession session = fixture.Start({.onlyPowerup = PowerupKind::Missile});
    session.GivePowerup(0, assets::BrickColour::Green, 1);
    RunUntilRacing(session);

    Run(session, 1, kUse);

    REQUIRE_FALSE(session.Powerups().Active().empty());
    for (const std::uint16_t slot : session.Powerups().Active()) {
        CHECK(session.Powerups().Slots()[slot].kind == PowerupKind::Missile);
    }
}

TEST_CASE("chaos bricks give the same random brick for the same seed") {
    const Fixture fixture;
    RaceSession first = fixture.Start({.hasChaosBricks = true}, 7);
    RaceSession second = fixture.Start({.hasChaosBricks = true}, 7);

    Run(first, 100);
    Run(second, 100);

    REQUIRE(first.Inventories()[0].Colour().has_value());
    CHECK(first.Inventories()[0] == second.Inventories()[0]);
}

TEST_CASE("chaos bricks draw several colours and white counts across seeds") {
    const Fixture fixture;
    std::vector<PowerupInventory> drawn;
    for (std::uint64_t seed = 1; seed <= 16; ++seed) {
        RaceSession session = fixture.Start({.hasChaosBricks = true}, seed);
        Run(session, 100);
        drawn.push_back(session.Inventories()[0]);
    }

    const auto differs = [&](const PowerupInventory& inventory) {
        return inventory.Colour() != drawn.front().Colour() ||
               inventory.WhiteCount() != drawn.front().WhiteCount();
    };
    CHECK(std::ranges::any_of(drawn, differs));
}

}
