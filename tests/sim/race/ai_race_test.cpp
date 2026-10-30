#include <doctest/doctest.h>

#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/track_loader.hpp"
#include "sim/collision/world_list.hpp"
#include "sim/race/race_session.hpp"
#include "sim/race/race_track.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr int kMaxTicks = 100000;
constexpr ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};

RaceSetup IdlePlayerAndComputers(std::size_t slotCount) {
    RaceSetup setup{.racers = {{.chassis = kChassis, .isPlayer = true}}};
    setup.racers.resize(slotCount, {.chassis = kChassis});
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

std::optional<std::string> RaceProblem(const assets::Vfs& vfs, const std::string& track) {
    Result<RaceTrack> loaded = LoadRaceTrack(vfs, track);
    if (!loaded) {
        return std::format("{}: {}", track, loaded.error().message);
    }
    const WorldList world(loaded->collision.Track());
    Result<RaceSession> session = RaceSession::Create(IdlePlayerAndComputers(loaded->grid.size()),
                                                      {.checkpoints = &loaded->checkpoints,
                                                       .grid = loaded->grid,
                                                       .world = &world,
                                                       .lowestTrackZ = loaded->lowestTrackZ,
                                                       .routes = &loaded->routes});
    if (!session) {
        return std::format("{}: {}", track, session.error().message);
    }
    const std::vector<RacerControls> idle(session->Racers().size());
    int ticks = 0;
    while (!HaveComputersFinished(*session) && ticks++ < kMaxTicks) {
        session->Tick(kTick, idle);
    }
    if (ticks > kMaxTicks) {
        return std::format("{}: the computer racers didn't all finish", track);
    }
    MESSAGE(
        std::format("{}: computer racers finished in {} s", track, ticks * kTick.count() / 1000));
    return std::nullopt;
}

TEST_CASE("five computer racers finish a three-lap race on every retail track") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    for (const std::string& track : assets::ListRaceTracks(*vfs)) {
        if (const std::optional<std::string> problem = RaceProblem(*vfs, track)) {
            FAIL_CHECK(*problem);
        }
    }
}

}
}
