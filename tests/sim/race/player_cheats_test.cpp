#include "sim/race/player_cheats.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "../ai/straight_route.hpp"
#include "../physics/plane_world.hpp"
#include "../powerup/fake_effect.hpp"
#include "checkpoint_ring.hpp"
#include "sim/race/race_session.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;

constexpr Milliseconds kTick{12};
constexpr ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
constexpr RacerControls kUse{.isUsePressed = true};
constexpr RacerControls kBoost{.isBoostPressed = true};
constexpr RacerControls kThrottle{.isThrottleHeld = true};
constexpr RacerControls kSkip{.isThrottleHeld = true, .isSkipPressed = true};
constexpr int kMostTicks = 2000;
constexpr std::size_t kPlayer = 0;
constexpr std::size_t kComputer = 1;

struct Fixture {
    CheckpointGraph graph = test::Ring();
    test::PlaneWorld world = test::FlatGround();
    std::array<RacerStart, 2> grid{RacerStart{.position = {.x = -20.0F, .z = 0.2F}},
                                   RacerStart{.position = {.x = -20.0F, .y = -10.0F, .z = 0.2F}}};
    GridRoutes routes{{test::StraightRoute({.x = -21.0F, .z = 0.2F})},
                      {test::StraightRoute({.x = -21.0F, .y = -10.0F, .z = 0.2F})}};
    assets::BrickLayout bricks;

    [[nodiscard]] RaceSession Start(const PlayerCheats& cheats) const {
        const RaceSetup setup{
            .racers = {{.chassis = kChassis, .isPlayer = true}, {.chassis = kChassis}},
            .seed = 1,
            .effects = test::FakeEffects(Milliseconds{60'000}),
            .playerCheats = cheats};
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

std::optional<std::size_t> LastGateOf(const RaceSession& session, std::size_t racer) {
    return session.Progress().OfRacer(racer).laps.LastCheckpoint();
}

void RunUntilFirstGate(RaceSession& session) {
    for (int tick = 0; tick < kMostTicks && !LastGateOf(session, kPlayer).has_value(); ++tick) {
        Run(session, 1, kThrottle);
    }
    REQUIRE(LastGateOf(session, kPlayer) == 0);
}

std::optional<TurboPhase> PhaseOf(const Racer& racer) {
    return racer.ActiveTurbo().transform([](const Turbo& turbo) { return turbo.phase; });
}

std::optional<TurboLevel> LevelOf(const Racer& racer) {
    return racer.ActiveTurbo().transform([](const Turbo& turbo) { return turbo.level; });
}

bool HasActive(const RaceSession& session, std::size_t racer, PowerupKind kind) {
    return std::ranges::any_of(session.Powerups().Active(), [&](std::uint16_t slot) {
        const EffectSlot& effect = session.Powerups().Slots()[slot];
        return effect.use.racer == racer && effect.kind == kind;
    });
}

}

TEST_CASE("invincibility goes to the player's racer only") {
    const Fixture fixture;
    const RaceSession session = fixture.Start({.isInvincible = true});

    CHECK(session.Racers()[kPlayer].IsInvincible());
    CHECK_FALSE(session.Racers()[kComputer].IsInvincible());
}

TEST_CASE("a bottomless brick keeps the player's power-up after a use") {
    const Fixture fixture;
    RaceSession session = fixture.Start({.hasBottomlessBrick = true});
    RunUntilRacing(session);
    session.GivePowerup(kPlayer, BrickColour::Green, 1);
    session.GivePowerup(kComputer, BrickColour::Green, 1);

    Run(session, 1, kUse);
    Run(session, 50);

    CHECK(HasActive(session, kPlayer, PowerupKind::Turbo));
    CHECK(session.Inventories()[kPlayer].Colour() == BrickColour::Green);
    CHECK(session.Inventories()[kPlayer].WhiteCount() == 1);
    CHECK_FALSE(session.Inventories()[kComputer].Colour().has_value());
}

TEST_CASE("full white keeps the player at 3 white bricks") {
    const Fixture fixture;
    RaceSession session = fixture.Start({.keepsFullWhite = true});

    Run(session, 1);
    CHECK(session.Inventories()[kPlayer].WhiteCount() == kMaxWhiteBricks);
    CHECK(session.Inventories()[kComputer].WhiteCount() == 0);

    session.GivePowerup(kPlayer, BrickColour::Red, 0);
    Run(session, 1);
    CHECK(session.Inventories()[kPlayer].Colour() == BrickColour::Red);
    CHECK(session.Inventories()[kPlayer].WhiteCount() == kMaxWhiteBricks);
}

TEST_CASE("a brick magnet lets the player pick up bricks from further away") {
    Fixture fixture;
    fixture.bricks.coloured = {
        {.position = {.x = -20.0F, .y = 12.0F, .z = 0.2F}, .colour = BrickColour::Red},
        {.position = {.x = -20.0F, .y = -22.0F, .z = 0.2F}, .colour = BrickColour::Red}};
    RaceSession session = fixture.Start({.hasBrickMagnet = true});

    Run(session, 500);

    CHECK(session.Inventories()[kPlayer].Colour() == BrickColour::Red);
    CHECK_FALSE(session.Inventories()[kComputer].Colour().has_value());
}

TEST_CASE("a long turbo stretches the player's boost three times") {
    const Fixture fixture;
    RaceSession session = fixture.Start({.hasLongTurbo = true});
    RunUntilRacing(session);
    for (Racer& racer : session.Racers()) {
        racer.StartTurbo(TurboLevel::Weak);
    }

    Run(session, 125);

    CHECK(PhaseOf(session.Racers()[kPlayer]) == TurboPhase::Boost);
    CHECK(PhaseOf(session.Racers()[kComputer]) == TurboPhase::Fade);
}

TEST_CASE("a perfect start gives the player the strong start boost without touching a pedal") {
    const Fixture fixture;
    RaceSession plain = fixture.Start({});
    RaceSession perfect = fixture.Start({.hasPerfectStart = true});

    RunUntilRacing(plain);
    RunUntilRacing(perfect);

    CHECK_FALSE(plain.Racers()[kPlayer].ActiveTurbo().has_value());
    CHECK(LevelOf(perfect.Racers()[kPlayer]) == TurboLevel::Strong);
    CHECK_FALSE(perfect.Racers()[kComputer].ActiveTurbo().has_value());
}

TEST_CASE("a free warp turns the player's green into a warp at no white bricks") {
    const Fixture fixture;
    RaceSession session = fixture.Start({.hasFreeWarp = true});
    RunUntilRacing(session);
    session.GivePowerup(kPlayer, BrickColour::Green, 0);
    session.GivePowerup(kComputer, BrickColour::Green, 0);

    Run(session, 1, kUse);
    Run(session, 50);

    CHECK(HasActive(session, kPlayer, PowerupKind::Warp));
    CHECK_FALSE(HasActive(session, kComputer, PowerupKind::Warp));
    CHECK(HasActive(session, kComputer, PowerupKind::Turbo));
}

TEST_CASE("the boost button fires a turbo only with the cheat on") {
    const Fixture fixture;
    RaceSession plain = fixture.Start({});
    RaceSession cheat = fixture.Start({.hasBoostButton = true});
    RunUntilRacing(plain);
    RunUntilRacing(cheat);

    Run(plain, 1, kBoost);
    Run(cheat, 1, kBoost);
    Run(cheat, 1);

    CHECK_FALSE(HasActive(plain, kPlayer, PowerupKind::Turbo));
    CHECK(HasActive(cheat, kPlayer, PowerupKind::Turbo));
    CHECK_FALSE(cheat.Inventories()[kPlayer].Colour().has_value());
}

TEST_CASE("skip ahead moves the player through the next gate and keeps their speed") {
    const Fixture fixture;
    RaceSession session = fixture.Start({.hasSkipAhead = true});
    RunUntilRacing(session);
    RunUntilFirstGate(session);
    const float speed = session.Racers()[kPlayer].Body().derived.speed;

    Run(session, 1, kSkip);

    const Racer& player = session.Racers()[kPlayer];
    CHECK(session.WasRespawned(kPlayer));
    CHECK(LastGateOf(session, kPlayer) == 1);
    CHECK(player.Body().motion.position.x >= 10.0F);
    CHECK(player.Body().derived.speed > speed / 2.0F);
}

TEST_CASE("skip ahead does nothing without the cheat") {
    const Fixture fixture;
    RaceSession session = fixture.Start({});
    RunUntilRacing(session);
    RunUntilFirstGate(session);

    Run(session, 1, kSkip);

    CHECK_FALSE(session.WasRespawned(kPlayer));
    CHECK(LastGateOf(session, kPlayer) == 0);
}

TEST_CASE("a head start holds the computer racers for the set time after the go") {
    const Fixture fixture;
    RaceSession session = fixture.Start({.headStart = Milliseconds{1200}});
    RunUntilRacing(session);
    const Vector3 start = session.Racers()[kComputer].Body().motion.position;

    Run(session, 99);
    CHECK_FALSE(session.Racers()[kPlayer].Boost().isHeldAtStart);
    CHECK(session.Racers()[kComputer].Boost().isHeldAtStart);
    CHECK(session.Racers()[kComputer].Body().motion.position == start);

    Run(session, 10);
    CHECK_FALSE(session.Racers()[kComputer].Boost().isHeldAtStart);
    CHECK_FALSE(session.Racers()[kComputer].Body().motion.position == start);
}

}
