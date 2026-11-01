#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "../../sim/physics/plane_world.hpp"
#include "../../sim/race/checkpoint_ring.hpp"
#include "app/hud/race_hud_values.hpp"
#include "app/race_step.hpp"
#include "sim/race/race_session.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kTick{12};
constexpr int kTicks = 1500;
constexpr sim::ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};

std::optional<std::string> Mismatch(const sim::RaceSession& session, std::size_t racer) {
    const HudValues values = ReadHudValues(session.Clock(), session.Progress(), racer);
    const sim::RacerProgress& own = session.Progress().OfRacer(racer);
    const int lapCount = session.Progress().LapCount();
    const int lap = std::min(own.laps.CompletedLaps() + 1, lapCount);
    if (values.lap != lap) {
        return std::format("lap {} instead of {}", values.lap, lap);
    }
    const std::span<const std::size_t> standings = session.Progress().Standings();
    const int place = own.finishPlace.value_or(
        static_cast<int>(std::ranges::find(standings, racer) - standings.begin()) + 1);
    if (values.place != place) {
        return std::format("place {} instead of {}", values.place, place);
    }
    if (values.lapTime && !std::ranges::contains(own.lapTimes, *values.lapTime)) {
        return std::format("lap time {} is no lap's time", values.lapTime->count());
    }
    if (values.isWrongWay != own.laps.IsWrongWay()) {
        return "wrong way differs";
    }
    return std::nullopt;
}

struct TwoRacerRace {
    sim::CheckpointGraph graph = test::Ring();
    test::PlaneWorld world = test::FlatGround();
    std::array<sim::RacerStart, 2> grid{
        sim::RacerStart{.position = {.x = -20.0F, .z = 0.2F}},
        sim::RacerStart{.position = {.x = -20.0F, .y = -10.0F, .z = 0.2F}}};

    [[nodiscard]] sim::RaceSession Start() const {
        Result<sim::RaceSession> session = sim::RaceSession::Create(
            {.racers = {{.chassis = kChassis, .isPlayer = true}, {.chassis = kChassis}}},
            {.checkpoints = &graph, .grid = grid, .world = &world, .lowestTrackZ = -100.0F});
        REQUIRE(session.has_value());
        return std::move(*session);
    }
};

TEST_CASE("HUD values match the race session on every tick") {
    const TwoRacerRace race;
    sim::RaceSession session = race.Start();
    const std::vector<sim::RacerControls> controls{{.isThrottleHeld = true}, {}};

    for (int tick = 0; tick < kTicks; ++tick) {
        session.Tick(kTick, controls);
        for (std::size_t racer = 0; racer < controls.size(); ++racer) {
            if (const std::optional<std::string> mismatch = Mismatch(session, racer)) {
                FAIL(std::format("tick {}, racer {}: {}", tick, racer, *mismatch));
            }
        }
    }
}

std::optional<std::size_t> RacerShownWrong(const sim::RenderSnapshot& snapshot,
                                           const sim::RaceSession& session) {
    for (std::size_t racer = 0; racer < snapshot.racers.size(); ++racer) {
        const sim::PowerupInventory& held = session.Inventories()[racer];
        const sim::RacerSnapshot& shown = snapshot.racers[racer];
        if (shown.heldColour != held.Colour() || shown.whiteCount != held.WhiteCount()) {
            return racer;
        }
    }
    return std::nullopt;
}

TEST_CASE("the snapshot carries each racer's power-up on every tick") {
    const TwoRacerRace race;
    sim::RaceSession session = race.Start();
    const std::vector<sim::RacerControls> controls{{.isThrottleHeld = true}, {}};
    sim::RenderSnapshot snapshot;

    for (int tick = 0; tick < kTicks; ++tick) {
        if (tick == kTicks / 3) {
            session.GivePowerup(0, assets::BrickColour::Blue, 2);
            session.GivePowerup(1, assets::BrickColour::Green, 0);
        }
        session.Tick(kTick, controls);
        SnapshotRace(session, snapshot);
        if (const std::optional<std::size_t> racer = RacerShownWrong(snapshot, session)) {
            FAIL(std::format("tick {}, racer {}: the snapshot's power-up differs", tick, *racer));
        }
    }
    CHECK(snapshot.racers[0].heldColour == assets::BrickColour::Blue);
    CHECK(snapshot.racers[0].whiteCount == 2);
}

}
}
