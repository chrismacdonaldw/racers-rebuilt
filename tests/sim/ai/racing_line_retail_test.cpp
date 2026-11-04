#include <doctest/doctest.h>

#include <chrono>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/race_route.hpp"
#include "assets/track_loader.hpp"
#include "sim/ai/racing_line.hpp"
#include "sim/collision/world_list.hpp"
#include "sim/race/race_session.hpp"
#include "sim/race/race_track.hpp"
#include "sim/race/racer_routes.hpp"

namespace rr::sim {
namespace {

constexpr float kProbeLift = 1.0F;

struct LineSurvey {
    std::size_t routes = 0;
    std::size_t closed = 0;
    std::size_t points = 0;
    std::size_t walledPoints = 0;
    std::size_t blockedSegments = 0;
    std::size_t recordedBlocked = 0;
    double fitMs = 0.0;
};

std::size_t BlockedSegments(std::span<const Vector3> points, const CollisionQuery& world) {
    std::size_t blocked = 0;
    const Vector3 lift{.z = kProbeLift};
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        blocked += CastSolid(world, points[i] + lift, points[i + 1] + lift) ? 1 : 0;
    }
    return blocked;
}

std::vector<Vector3> RecordedPoints(const assets::RaceRoute& route) {
    std::vector<Vector3> points;
    Vector3 position = route.startPosition;
    for (const assets::RoutePoint& point : route.points) {
        position += point.offset;
        points.push_back(position);
    }
    return points;
}

LineSurvey SurveyTrack(const RaceTrack& track, const CollisionQuery& world) {
    LineSurvey survey;
    RaceSetup setup;
    setup.aiDriving = AiDriving::Physics;
    setup.aiCraft.racingLine = true;
    setup.racers.resize(track.routes.size());
    const RaceCourse course{.grid = track.grid, .world = &world, .routes = &track.routes};
    const auto start = std::chrono::steady_clock::now();
    const RacerRoutes routes(setup, course);
    survey.fitMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    for (std::size_t slot = 0; slot < track.routes.size(); ++slot) {
        for (std::size_t variant = 0; variant < track.routes[slot].size(); ++variant) {
            const RacingLine& line = *routes.LinesOf(slot)[variant];
            ++survey.routes;
            survey.closed += line.IsClosed() ? 1 : 0;
            survey.points += line.Points().size();
            survey.walledPoints += line.WalledPoints();
            survey.blockedSegments += BlockedSegments(line.Points(), world);
            survey.recordedBlocked +=
                BlockedSegments(RecordedPoints(track.routes[slot][variant]), world);
        }
    }
    return survey;
}

TEST_CASE("survey: racing lines on every retail route stay clear of walls" * doctest::skip()) {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    for (const std::string& name : assets::ListRaceTracks(*vfs)) {
        Result<RaceTrack> track = LoadRaceTrack(*vfs, name, false);
        REQUIRE(track.has_value());
        const WorldList world(track->collision.Track());
        const LineSurvey survey = SurveyTrack(*track, world);
        MESSAGE(
            std::format("{}: {} routes ({} closed), {} points, {} walled, {} line and {} recorded "
                        "segments blocked, {:.1f} ms",
                        name, survey.routes, survey.closed, survey.points, survey.walledPoints,
                        survey.blockedSegments, survey.recordedBlocked, survey.fitMs));
    }
}

}
}
