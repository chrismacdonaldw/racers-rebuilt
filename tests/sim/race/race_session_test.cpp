#include "sim/race/race_session.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "../ai/straight_route.hpp"
#include "../physics/plane_world.hpp"
#include "../powerup/fake_effect.hpp"
#include "checkpoint_ring.hpp"
#include "sim/powerup/green_effects.hpp"
#include "sim/powerup/red_effects.hpp"
#include "sim/powerup/shield_effect.hpp"

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
    const assets::BrickLayout* bricks = nullptr;

    [[nodiscard]] RaceSession Start(const RaceSetup& setup) const {
        Result<RaceSession> session = RaceSession::Create(setup, {.checkpoints = &graph,
                                                                  .grid = grid,
                                                                  .world = &world,
                                                                  .lowestTrackZ = -100.0F,
                                                                  .routes = &routes,
                                                                  .bricks = bricks});
        REQUIRE(session.has_value());
        return std::move(*session);
    }
};

constexpr PowerupHabits kNeverUses{.useChances = {}};

RaceSetup TwoRacers(RespawnRule respawn = {}) {
    return {.racers = {{.chassis = kChassis, .isPlayer = true},
                       {.chassis = kChassis, .habits = kNeverUses}},
            .respawn = respawn};
}

void Run(RaceSession& session, int ticks, const RacerControls& controls = kThrottle) {
    const std::vector<RacerControls> all(session.Racers().size(), controls);
    for (int tick = 0; tick < ticks; ++tick) {
        session.Tick(kTick, all);
    }
}

using HeardSound = std::pair<int, SoundEvent>;

std::vector<HeardSound> Listen(RaceSession& session, int ticks, const RacerControls& controls) {
    std::vector<HeardSound> heard;
    for (int tick = 0; tick < ticks; ++tick) {
        Run(session, 1, controls);
        for (const SoundEvent& sound : session.Sounds()) {
            heard.emplace_back(tick, sound);
        }
    }
    return heard;
}

std::vector<HeardSound> WallHits(const std::vector<HeardSound>& heard) {
    std::vector<HeardSound> hits;
    std::ranges::copy_if(heard, std::back_inserter(hits), [](const HeardSound& sound) {
        return sound.second.sound == RaceSound::WallHit;
    });
    return hits;
}

int ShortestGap(const std::vector<HeardSound>& hits) {
    int gap = std::numeric_limits<int>::max();
    for (std::size_t i = 1; i < hits.size(); ++i) {
        gap = std::min(gap, hits[i].first - hits[i - 1].first);
    }
    return gap;
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

TEST_CASE("the countdown sounds fire on the ticks the countdown reaches them") {
    constexpr int kIntroTicks = 167;
    constexpr int kTicksTo1000Ms = 84;
    constexpr int kTicksTo2000Ms = 167;
    constexpr int kTicksTo3000Ms = 250;
    const Fixture fixture;
    RaceSession session = fixture.Start(TwoRacers());

    const std::vector<HeardSound> heard = Listen(session, kIntroTicks + kTicksTo3000Ms, {});

    constexpr int kStart = kIntroTicks - 1;
    CHECK(session.Clock().Phase() == RacePhase::Racing);
    CHECK(heard == std::vector<HeardSound>{
                       {kStart, {.sound = RaceSound::CountdownBeep}},
                       {kStart, {.sound = RaceSound::EngineStart}},
                       {kStart + kTicksTo1000Ms, {.sound = RaceSound::CountdownBeep}},
                       {kStart + kTicksTo2000Ms, {.sound = RaceSound::CountdownBeep}},
                       {kStart + kTicksTo3000Ms, {.sound = RaceSound::Go}},
                   });
}

TEST_CASE("a turbo sounds on the tick it starts and on the tick it fades") {
    const Fixture fixture;
    RaceSession session = fixture.Start(TwoRacers());
    while (session.Clock().Phase() != RacePhase::Racing) {
        Run(session, 1, {});
    }

    const std::vector<HeardSound> heard = Listen(session, 200, kThrottle);

    constexpr int kFadeTick = 158;
    const SoundEvent start{.sound = RaceSound::TurboStart, .level = TurboLevel::Strong};
    const SoundEvent fade{.sound = RaceSound::TurboFade, .level = TurboLevel::Strong};
    CHECK(heard == std::vector<HeardSound>{{0, start}, {kFadeTick, fade}});
}

TEST_CASE("a racer against a wall hits it at most once every 250 ms") {
    Fixture fixture;
    fixture.world = test::PlaneWorld({test::TestPlane{}, test::TestPlane{.normal = {.x = -1.0F}}});
    RaceSession session = fixture.Start(TwoRacers());
    RunUntilRacing(session);

    const std::vector<HeardSound> hits = WallHits(Listen(session, 400, kThrottle));

    REQUIRE(hits.size() > 1);
    CHECK(std::ranges::all_of(hits, [](const HeardSound& hit) { return hit.second.racer == 0; }));
    CHECK(ShortestGap(hits) * kTick >= Milliseconds{250});
}

TEST_CASE("a racer off the ground respawns on its grid slot after the timeout") {
    Fixture fixture;
    fixture.grid[0].position.z = kHighUp;
    RaceSession session = fixture.Start(TwoRacers({.timeout = Milliseconds{1000}}));

    Run(session, 83);
    CHECK_FALSE(session.WasRespawned(0));
    Run(session, 1);
    CHECK(session.WasRespawned(0));
    Run(session, 8);

    const Vector3& position = session.Racers()[0].Body().motion.position;
    CHECK(position.x == doctest::Approx(-20.0F));
    CHECK(position.z < 1.0F);
    CHECK_FALSE(session.WasRespawned(0));
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

TEST_CASE("a racer takes a brick inside its bounds, wherever they sit on the car") {
    Fixture fixture;
    const assets::BrickLayout bricks{
        .coloured = {{.position = {.x = -20.0F, .z = 5.2F}, .colour = assets::BrickColour::Red}}};
    fixture.bricks = &bricks;
    RaceSetup setup = TwoRacers();
    setup.racers[0].localBounds = {.centre = {.z = 5.0F}, .radius = 0.5F};
    setup.racers[1].localBounds = {.radius = 0.5F};
    RaceSession session = fixture.Start(setup);

    Run(session, 100, {});

    CHECK(session.Inventories()[0].Colour() == assets::BrickColour::Red);
    CHECK_FALSE(session.Inventories()[1].Colour().has_value());
}

TEST_CASE("bricks stay silent until racing starts") {
    constexpr int kTicksTo7Seconds = 584;
    Fixture fixture;
    const assets::BrickLayout bricks{
        .coloured = {{.position = {.z = kHighUp}},
                     {.position = {.z = kHighUp}, .respawn = Milliseconds{10000}}}};
    fixture.bricks = &bricks;
    RaceSession session = fixture.Start(TwoRacers());

    const std::vector<HeardSound> heard = Listen(session, kTicksTo7Seconds, {});

    std::vector<SoundEvent> respawns;
    for (const HeardSound& sound : heard) {
        if (sound.second.sound == RaceSound::BrickRespawn) {
            respawns.push_back(sound.second);
        }
    }
    CHECK(respawns == std::vector<SoundEvent>{{.sound = RaceSound::BrickRespawn, .brick = 1}});
}

TEST_CASE("pressing use fires the held power-up once racing has started") {
    const Fixture fixture;
    RaceSetup setup = TwoRacers();
    setup.effects = test::FakeEffects(Milliseconds{60'000});
    RaceSession session = fixture.Start(setup);
    session.GivePowerup(0, assets::BrickColour::Green, 1);
    constexpr RacerControls kUse{.isUsePressed = true};

    Run(session, 1, kUse);
    CHECK(session.Powerups().Active().empty());
    CHECK(session.Inventories()[0].Colour() == assets::BrickColour::Green);

    RunUntilRacing(session);
    Run(session, 1, kUse);

    REQUIRE(session.Powerups().Active().size() == 1);
    const EffectSlot& turbo = session.Powerups().Slots()[session.Powerups().Active()[0]];
    CHECK(turbo.kind == PowerupKind::Turbo);
    CHECK(turbo.use.level == 1);
    CHECK(session.Inventories()[0] == PowerupInventory{});
}

TEST_CASE("a computer racer that rolls under its green chance starts with a level 0 turbo") {
    const Fixture fixture;
    RaceSetup setup = TwoRacers();
    setup.racers[1].habits = {};
    setup.effects = test::FakeEffects(Milliseconds{60'000});
    RaceSession session = fixture.Start(setup);

    RunUntilRacing(session);

    REQUIRE(session.Powerups().Active().size() == 1);
    const EffectSlot& boost = session.Powerups().Slots()[session.Powerups().Active()[0]];
    CHECK(boost.kind == PowerupKind::Turbo);
    CHECK(boost.use.racer == 1);
    CHECK(boost.use.level == 0);
}

TEST_CASE("a computer racer uses the power-up it holds once racing starts") {
    const Fixture fixture;
    RaceSetup setup = TwoRacers();
    setup.racers[1].habits = {.useChances = {kSureUseChance, kSureUseChance, 0, kSureUseChance}};
    setup.effects = test::FakeEffects(Milliseconds{60'000});
    RaceSession session = fixture.Start(setup);
    session.GivePowerup(1, assets::BrickColour::Yellow, 0);
    RunUntilRacing(session);
    CHECK(session.Inventories()[1].Colour() == assets::BrickColour::Yellow);

    Run(session, kTicksTo15Seconds / 10, {});

    CHECK(session.Inventories()[1] == PowerupInventory{});
    REQUIRE(session.Powerups().Active().size() == 1);
    CHECK(session.Powerups().Slots()[session.Powerups().Active()[0]].use.racer == 1);
}

TEST_CASE("a warp sounds as it opens and as it lands") {
    const Fixture fixture;
    RaceSetup setup = TwoRacers();
    AddGreenEffects(setup.effects);
    RaceSession session = fixture.Start(setup);
    RunUntilRacing(session);
    session.GivePowerup(0, assets::BrickColour::Green, 3);

    const std::vector<HeardSound> heard = Listen(session, 200, {.isUsePressed = true});

    std::vector<HeardSound> warps;
    std::ranges::copy_if(heard, std::back_inserter(warps), [](const HeardSound& sound) {
        return sound.second.sound == RaceSound::WarpEnter ||
               sound.second.sound == RaceSound::WarpLand;
    });
    CHECK(warps == std::vector<HeardSound>{{41, {.sound = RaceSound::WarpEnter}},
                                           {166, {.sound = RaceSound::WarpLand}}});
}

TEST_CASE("a cannonball sounds as it fires and as it lands, then explodes") {
    const Fixture fixture;
    RaceSetup setup = TwoRacers();
    AddRedEffects(setup.effects);
    RaceSession session = fixture.Start(setup);
    RunUntilRacing(session);
    session.GivePowerup(0, assets::BrickColour::Red, 0);

    const std::vector<HeardSound> heard = Listen(session, 300, {.isUsePressed = true});

    std::vector<RaceSound> cannon;
    for (const HeardSound& sound : heard) {
        if (sound.second.sound == RaceSound::CannonFire ||
            sound.second.sound == RaceSound::CannonHit) {
            cannon.push_back(sound.second.sound);
        }
    }
    CHECK(cannon == std::vector{RaceSound::CannonFire, RaceSound::CannonHit});
    CHECK(std::ranges::any_of(session.Blasts().All(), [](const Explosion& explosion) {
        return explosion.phase != ExplosionPhase::Idle;
    }));
}

TEST_CASE("a player beside a computer racer is pushed clear of it") {
    Fixture fixture;
    fixture.grid[1].position = {.x = -20.0F, .y = -4.5F, .z = 0.2F};
    fixture.routes = {{test::StraightRoute({.x = -21.0F, .z = 0.2F})},
                      {test::StraightRoute({.x = -20.0F, .y = -4.5F, .z = 0.2F})}};
    RaceSession session = fixture.Start(TwoRacers());

    Run(session, 1, {});

    const float gap =
        session.Racers()[0].Body().motion.position.y - session.Racers()[1].Body().motion.position.y;
    CHECK(gap == doctest::Approx(5.0F).epsilon(0.01));
}

TEST_CASE("a computer racer that drives into a level 2 shield spins") {
    Fixture fixture;
    fixture.routes = {{test::StraightRoute({.x = -21.0F, .z = 0.2F})},
                      {test::StraightRoute({.x = -30.0F, .z = 0.2F})}};
    RaceSetup setup = TwoRacers();
    AddBlueEffects(setup.effects);
    RaceSession session = fixture.Start(setup);
    session.GivePowerup(0, assets::BrickColour::Blue, 2);
    RunUntilRacing(session);

    Run(session, 1, {.isUsePressed = true});
    REQUIRE(session.Racers()[0].IsShielded());
    int ticks = 0;
    while (!session.Racers()[1].IsSpinning() && ticks < 250) {
        Run(session, 1, {});
        ++ticks;
    }

    CHECK(session.Racers()[1].IsSpinning());
    CHECK_FALSE(session.Racers()[0].IsSpinning());
}

std::optional<Vector3> TauntAt(const RaceSession& session) {
    const auto taunt =
        std::ranges::find_if(session.Triggers().Events(), [](const TrackEvent& event) {
            return event.id == kTauntEvent && event.edge == EventEdge::Start;
        });
    return taunt == session.Triggers().Events().end() ? std::nullopt : taunt->position;
}

TEST_CASE("a computer racer close behind another racer fires the taunt event where it is") {
    const Fixture fixture;
    RaceSession session = fixture.Start(TwoRacers());
    RunUntilRacing(session);

    std::optional<Vector3> taunt;
    Vector3 computer;
    for (int tick = 0; tick < 200 && !taunt; ++tick) {
        const BodyMotion& motion = session.Racers()[1].Body().motion;
        session.Racers()[0].Place({.position = motion.position + (motion.basis.forward * 11.0F)});
        Run(session, 1, {});
        computer = session.Racers()[1].Body().motion.position;
        taunt = TauntAt(session);
    }

    REQUIRE(taunt);
    CHECK(Length(*taunt - computer) < 0.001F);
}

TEST_CASE("a player pressing use with nothing held fires the taunt event where it is") {
    const Fixture fixture;
    RaceSession session = fixture.Start(TwoRacers());
    RunUntilRacing(session);

    Run(session, 1, {.isUsePressed = true});

    const std::optional<Vector3> taunt = TauntAt(session);
    REQUIRE(taunt);
    CHECK(Length(*taunt - session.Racers()[0].Body().motion.position) < 0.001F);
}

}
}
