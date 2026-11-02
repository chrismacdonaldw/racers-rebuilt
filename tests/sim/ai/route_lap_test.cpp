#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/race_catalog.hpp"
#include "assets/racer_catalog.hpp"
#include "assets/track_loader.hpp"
#include "sim/ai/route_driver.hpp"
#include "sim/collision/world_list.hpp"
#include "sim/physics/racer_chassis.hpp"
#include "sim/race/race_progress.hpp"
#include "sim/race/race_track.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr int kLapCount = 2;
constexpr int kMaxTicks = 30000;
constexpr float kCruisePercentile = 0.98F;
constexpr double kMedianTolerance = 0.02;
constexpr double kTrackTolerance = 0.07;

struct Lap {
    Milliseconds time{0};
    std::vector<float> groundedSpeeds;
};

RacerStart StartOf(const assets::RaceRoute& route) {
    const Vector3 forward = Rotate(route.startRotation, {.x = 1.0F});
    return {.position = route.startPosition, .headingRadians = std::atan2(forward.y, forward.x)};
}

template <typename Controls>
Lap FlyingLap(Racer& racer, const RaceTrack& track, const CollisionQuery& world,
              Controls controls) {
    RaceProgress progress(1, kLapCount);
    Lap lap;
    for (int tick = 0; tick < kMaxTicks && !progress.OfRacer(0).finishPlace; ++tick) {
        const Vector3 from = racer.Body().motion.position;
        racer.Tick(kTick, controls(racer), world);
        const BodyState& body = racer.Body();
        progress.Move(track.checkpoints, 0,
                      {.from = from, .to = body.motion.position, .facing = body.facing}, kTick,
                      true);
        if (progress.OfRacer(0).laps.CompletedLaps() == 1 && body.motion.contactCount >= 3) {
            lap.groundedSpeeds.push_back(body.derived.speed);
        }
    }
    if (progress.OfRacer(0).finishPlace) {
        lap.time = progress.OfRacer(0).lapTimes.back();
    }
    return lap;
}

int TopSpeedStatFor(std::vector<float> speeds) {
    std::ranges::sort(speeds);
    const auto index =
        static_cast<std::size_t>(kCruisePercentile * static_cast<float>(speeds.size() - 1));
    const float multiplier = speeds.at(index) * 1000.0F / kBaseMaxSpeed;
    return std::clamp(static_cast<int>(std::lround(50.0F + ((multiplier - 1.0F) / 0.001F))), 0,
                      100);
}

Result<double> LapRatio(const assets::Vfs& vfs, const assets::RacerCatalog& catalog,
                        const std::string& name) {
    Result<RaceTrack> track = LoadRaceTrack(vfs, name);
    Result<assets::RaceRoute> route =
        track.and_then([&](const RaceTrack&) { return LoadRoute(vfs, name, "R1_F_0.RRB", false); });
    Result<assets::RacerFiles> files =
        route.and_then([&](const assets::RaceRoute&) { return assets::LoadGrid(vfs, name, ""); })
            .and_then(
                [&](const std::vector<std::string>& grid) { return catalog.Find(grid.at(1)); });
    if (!files) {
        return std::unexpected(files.error());
    }
    ChassisSpec chassis = PrebuiltChassis(files->champion, files->chassis);
    const WorldList world(track->collision.Track());

    Racer recorded(chassis, {}, StartOf(*route));
    recorded.FollowRoute(*route);
    const Lap recordedLap =
        FlyingLap(recorded, *track, world, [](const Racer&) { return RacerControls{}; });

    if (recordedLap.groundedSpeeds.empty()) {
        return Fail(ErrorCode::InvalidData, std::format("{}: the recording didn't lap", name));
    }
    chassis.topSpeed = TopSpeedStatFor(recordedLap.groundedSpeeds);
    Racer driven(chassis, {}, StartOf(*route));
    RouteDriver driver(*route);
    const Lap drivenLap =
        FlyingLap(driven, *track, world, [&](const Racer& racer) { return driver.Drive(racer); });
    if (drivenLap.time == Milliseconds{0}) {
        return Fail(ErrorCode::InvalidData, std::format("{}: the driven car didn't finish", name));
    }
    const double ratio =
        static_cast<double>(drivenLap.time.count()) / static_cast<double>(recordedLap.time.count());
    MESSAGE(std::format("{}: recorded {} ms, driven {} ms ({:+.1f}%)", name,
                        recordedLap.time.count(), drivenLap.time.count(), (ratio - 1.0) * 100.0));
    return ratio;
}

struct TrackLaps {
    std::vector<double> errors;
    std::vector<std::string> failures;
};

TrackLaps MeasureTracks(const assets::Vfs& vfs, const assets::RacerCatalog& catalog) {
    TrackLaps laps;
    for (const std::string& track : assets::ListRaceTracks(vfs)) {
        const Result<double> ratio = LapRatio(vfs, catalog, track);
        if (!ratio) {
            laps.failures.push_back(std::format("{}: {}", track, ratio.error().message));
            continue;
        }
        const double error = std::abs(*ratio - 1.0);
        if (error > kTrackTolerance) {
            laps.failures.push_back(std::format("{}: off by {:.1f}%", track, error * 100.0));
        }
        laps.errors.push_back(error);
    }
    return laps;
}

double Median(std::vector<double> values) {
    const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
    std::ranges::nth_element(values, middle);
    return *middle;
}

TEST_CASE("a car driving route R1_F_0 with the physics laps close to the recording") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    Result<assets::RacerCatalog> catalog = assets::RacerCatalog::Load(*vfs);
    REQUIRE(catalog);

    const TrackLaps laps = MeasureTracks(*vfs, *catalog);

    for (const std::string& failure : laps.failures) {
        FAIL_CHECK(failure);
    }
    REQUIRE_FALSE(laps.errors.empty());
    CHECK(Median(laps.errors) <= kMedianTolerance);
}

}
}
