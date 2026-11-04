#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/track_loader.hpp"
#include "sim/ai/gate_path.hpp"
#include "sim/collision/world_list.hpp"
#include "sim/race/race_track.hpp"

namespace rr::sim {
namespace {

constexpr float kProbeLift = 1.0F;
constexpr float kLevelRise = 2.0F;

struct LoopSurvey {
    std::size_t points = 0;
    std::size_t airborne = 0;
    std::size_t blocked = 0;
    std::size_t missedGates = 0;
    double traceMs = 0.0;
};

std::size_t BlockedSegments(std::span<const GatePoint> points, const CollisionQuery& world) {
    std::size_t blocked = 0;
    const Vector3 lift{.z = kProbeLift};
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vector3& from = points[i].position;
        const Vector3& to = points[(i + 1) % points.size()].position;
        if (std::abs(to.z - from.z) < kLevelRise && CastSolid(world, from + lift, to + lift)) {
            ++blocked;
        }
    }
    return blocked;
}

std::size_t MissedGates(const CheckpointGraph& graph, std::span<const GatePoint> points) {
    std::vector<bool> crossed(graph.Checkpoints().size(), false);
    for (std::size_t i = 0; i < points.size(); ++i) {
        const std::optional<GateCrossing> crossing =
            graph.FindCrossing(points[i].position, points[(i + 1) % points.size()].position);
        if (crossing && crossing->side == GateSide::Forward) {
            crossed[crossing->checkpoint] = true;
        }
    }
    std::size_t missed = 0;
    std::size_t gate = 0;
    do {
        missed += crossed[gate] ? 0 : 1;
        gate = graph.Checkpoints()[gate].links[0];
    } while (gate != 0);
    return missed;
}

LoopSurvey SurveyTrack(const assets::Vfs& vfs, const std::string& name) {
    const Result<RaceTrack> track = LoadRaceTrack(vfs, name, false);
    if (!track) {
        throw std::runtime_error(track.error().message);
    }
    const WorldList world(track->collision.Track());
    const auto start = std::chrono::steady_clock::now();
    const std::optional<GateLoop> loop = GateLoop::Trace(track->checkpoints.Checkpoints(), world);
    const auto end = std::chrono::steady_clock::now();
    if (!loop) {
        throw std::runtime_error("the checkpoints trace no loop");
    }
    return {.points = loop->Points().size(),
            .airborne = static_cast<std::size_t>(
                std::ranges::count_if(loop->Points(), &GatePoint::isAirborne)),
            .blocked = BlockedSegments(loop->Points(), world),
            .missedGates = MissedGates(track->checkpoints, loop->Points()),
            .traceMs = std::chrono::duration<double, std::milli>(end - start).count()};
}

TEST_CASE("every retail track traces a gate loop that passes each of its gates") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    for (const std::string& name : assets::ListRaceTracks(*vfs)) {
        CAPTURE(name);
        const LoopSurvey survey = SurveyTrack(*vfs, name);
        INFO(std::format("{} points, {} airborne, {} blocked, {} gates missed, {:.1f} ms",
                         survey.points, survey.airborne, survey.blocked, survey.missedGates,
                         survey.traceMs));
        CHECK(survey.missedGates == 0);
    }
}

}
}
