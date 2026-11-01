#include <doctest/doctest.h>

#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/circuit_database.hpp"
#include "assets/racer_catalog.hpp"
#include "assets/track_loader.hpp"
#include "sim/collision/world_list.hpp"
#include "sim/powerup/green_effects.hpp"
#include "sim/powerup/red_effects.hpp"
#include "sim/powerup/shield_effect.hpp"
#include "sim/powerup/yellow_effects.hpp"
#include "sim/race/race_session.hpp"
#include "sim/race/race_track.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr int kMaxTicks = 100000;
constexpr std::size_t kMinPowerupUsers = 3;
constexpr ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};

struct PowerupTally {
    std::vector<bool> pickedUp;
    std::vector<bool> used;
    std::size_t startBoosts = 0;

    explicit PowerupTally(std::size_t racers) : pickedUp(racers), used(racers) {}

    void Count(const RaceSession& session, bool isStart) {
        for (std::size_t i = 0; i < pickedUp.size(); ++i) {
            pickedUp[i] = pickedUp[i] || session.Inventories()[i].Colour().has_value();
        }
        for (const PowerupEvent& event : session.Powerups().Events()) {
            used[event.use.racer] = used[event.use.racer] || event.kind == PowerupEventKind::Used;
            startBoosts += isStart && event.kind == PowerupEventKind::Started ? 1 : 0;
        }
    }

    [[nodiscard]] std::size_t ComputersThatUsedPowerups() const {
        std::size_t count = 0;
        for (std::size_t i = 1; i < used.size(); ++i) {
            count += pickedUp[i] && used[i] ? 1 : 0;
        }
        return count;
    }
};

Result<RaceSetup> CircuitRace(const assets::Vfs& vfs, const std::string& track,
                              std::size_t slotCount) {
    Result<assets::RacerCatalog> catalog = assets::RacerCatalog::Load(vfs);
    Result<std::vector<std::string>> codes =
        catalog.and_then([&](const assets::RacerCatalog&) { return assets::LoadGrid(vfs, track); });
    if (!codes) {
        return std::unexpected(codes.error());
    }
    RaceSetup setup{.racers = {{.chassis = kChassis, .isPlayer = true}}};
    AddGreenEffects(setup.effects);
    AddBlueEffects(setup.effects);
    AddRedEffects(setup.effects);
    AddYellowEffects(setup.effects);
    for (std::size_t i = 1; i < slotCount; ++i) {
        Result<assets::RacerFiles> files = catalog->Find(codes->at(i));
        if (!files) {
            return std::unexpected(files.error());
        }
        setup.racers.push_back({.chassis = kChassis, .habits = HabitsOf(files->driver)});
    }
    return setup;
}

bool HaveComputersFinished(const RaceSession& session) {
    for (std::size_t i = 1; i < session.Racers().size(); ++i) {
        if (!session.Progress().OfRacer(i).finishPlace) {
            return false;
        }
    }
    return true;
}

std::optional<std::string> RunRace(RaceSession& session, const std::string& track,
                                   std::size_t& startBoosts) {
    const std::vector<RacerControls> idle(session.Racers().size());
    PowerupTally tally(session.Racers().size());
    int ticks = 0;
    while (!HaveComputersFinished(session) && ticks++ < kMaxTicks) {
        const bool wasRacing = session.Clock().Phase() == RacePhase::Racing;
        session.Tick(kTick, idle);
        tally.Count(session, !wasRacing && session.Clock().Phase() == RacePhase::Racing);
    }
    if (ticks > kMaxTicks) {
        return std::format("{}: the computer racers didn't all finish", track);
    }
    if (tally.ComputersThatUsedPowerups() < kMinPowerupUsers) {
        return std::format("{}: only {} computer racers picked up and used a power-up", track,
                           tally.ComputersThatUsedPowerups());
    }
    startBoosts += tally.startBoosts;
    MESSAGE(std::format("{}: computer racers finished in {} s, {} with a start boost", track,
                        ticks * kTick.count() / 1000, tally.startBoosts));
    return std::nullopt;
}

std::optional<std::string> RaceProblem(const assets::Vfs& vfs, const std::string& track,
                                       std::size_t& startBoosts) {
    Result<RaceTrack> loaded = LoadRaceTrack(vfs, track);
    Result<RaceSetup> setup = loaded.and_then(
        [&](const RaceTrack& race) { return CircuitRace(vfs, track, race.grid.size()); });
    if (!setup) {
        return std::format("{}: {}", track, setup.error().message);
    }
    const WorldList world(loaded->collision.Track());
    Result<RaceSession> session = RaceSession::Create(*setup, {.checkpoints = &loaded->checkpoints,
                                                               .grid = loaded->grid,
                                                               .world = &world,
                                                               .lowestTrackZ = loaded->lowestTrackZ,
                                                               .routes = &loaded->routes,
                                                               .bricks = &loaded->bricks,
                                                               .targets = loaded->targets});
    if (!session) {
        return std::format("{}: {}", track, session.error().message);
    }
    return RunRace(*session, track, startBoosts);
}

TEST_CASE("on every retail track, most computer racers use power-ups and all finish three laps") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    std::size_t startBoosts = 0;
    for (const std::string& track : assets::ListRaceTracks(*vfs)) {
        if (const std::optional<std::string> problem = RaceProblem(*vfs, track, startBoosts)) {
            FAIL_CHECK(*problem);
        }
    }
    CHECK(startBoosts > 0);
}

}
}
