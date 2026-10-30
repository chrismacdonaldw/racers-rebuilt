#include "app/checkpoint_gates.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "app/pace_car.hpp"
#include "app/track_space.hpp"
#include "assets/archive_survey.hpp"
#include "assets/jam_archive.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "sim/race/checkpoint_ring.hpp"
#include "sim/race/race_track.hpp"
#include "sim/route_playback.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kTick{12};
constexpr int kLaps = 2;

struct Lapping {
    const sim::CheckpointGraph* graph = nullptr;
    sim::LapTracker laps;
    bool wasWrongWay = false;

    void Move(const Transform& from, const Transform& to, Milliseconds elapsed, float facing) {
        if (const auto crossing = graph->FindCrossing(from.position, to.position)) {
            laps.Cross(*graph, *crossing);
        }
        laps.Update(*graph, elapsed, Apply({.rotation = to.rotation}, {.x = facing}));
        wasWrongWay = wasWrongWay || laps.IsWrongWay();
    }

    void Drive(sim::RoutePlayback& route, Milliseconds duration) {
        while (duration > Milliseconds{0}) {
            const Milliseconds step = std::min(kTick, duration);
            const Transform from = route.Pose();
            route.Advance(step);
            Move(from, route.Pose(), step, 1.0F);
            duration -= step;
        }
    }
};

Milliseconds RouteLength(const assets::RaceRoute& route) {
    return Milliseconds{std::accumulate(route.points.begin(), route.points.end(), std::int64_t{0},
                                        [](std::int64_t total, const assets::RoutePoint& point) {
                                            return total + point.durationMs;
                                        })};
}

std::vector<Transform> Samples(sim::RoutePlayback route, Milliseconds duration) {
    std::vector<Transform> poses{route.Pose()};
    for (Milliseconds at{0}; at < duration; at += kTick) {
        route.Advance(kTick);
        poses.push_back(route.Pose());
    }
    return poses;
}

std::optional<assets::Vfs> RetailVfs() {
    std::optional<assets::JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return std::nullopt;
    }
    assets::Vfs vfs;
    vfs.Mount(std::make_unique<assets::JamArchive>(std::move(*archive)));
    return vfs;
}

constexpr std::size_t kLinesPerGate = 4;

bool IsSameColor(std::span<const render::DebugLine> lines, std::size_t a, std::size_t b) {
    const render::Color& first = lines[kLinesPerGate * a].color;
    const render::Color& second = lines[kLinesPerGate * b].color;
    return first.r == second.r && first.g == second.g && first.b == second.b;
}

TEST_CASE("each gate draws as a frame, with the last and next gates highlighted") {
    const sim::CheckpointGraph graph = test::Ring();
    sim::LapTracker laps;
    laps.Cross(graph, {.checkpoint = 1});
    std::vector<render::DebugLine> lines;

    DrawGates(graph, laps, lines);

    REQUIRE(lines.size() == kLinesPerGate * graph.Checkpoints().size());
    const bool highlightsLastAndNext = IsSameColor(lines, 2, 4) && !IsSameColor(lines, 0, 1) &&
                                       !IsSameColor(lines, 0, 2) && !IsSameColor(lines, 1, 2);
    CHECK(highlightsLastAndNext);
    const bool spansGate = lines[0].from == ToEngine({.y = sim::kGateRadius}) &&
                           lines[0].to == ToEngine({.y = -sim::kGateRadius});
    CHECK(spansGate);
}

std::vector<std::string> PaceCarProblems(const assets::Vfs& vfs, const std::string& track) {
    const std::string folder = "GAMEDATA/" + track;
    Result<sim::CheckpointGraph> graph = sim::LoadCheckpoints(vfs, track);
    Result<assets::RaceRoute> route = LoadPaceRoute(vfs, folder);
    if (!graph || !route) {
        return {std::format("{}: {}", track, (graph ? route.error() : graph.error()).message)};
    }
    const Milliseconds length = RouteLength(*route);
    const Milliseconds lap = length - Milliseconds{route->loopTimeMs};

    sim::RoutePlayback playback(*route);
    Lapping run{.graph = &*graph};
    run.Drive(playback, length);
    const int lapsBefore = run.laps.CompletedLaps();
    run.Drive(playback, kLaps * lap);
    std::vector<std::string> problems;
    if (run.laps.CompletedLaps() != lapsBefore + kLaps) {
        problems.push_back(std::format("{}: {} laps in {} rounds", track,
                                       run.laps.CompletedLaps() - lapsBefore, kLaps));
    }
    if (run.wasWrongWay) {
        problems.push_back(std::format("{}: wrong way while driving forward", track));
    }

    const float progress = run.laps.Progress(*graph);
    const std::vector<Transform> poses = Samples(playback, lap);
    for (std::size_t i = poses.size() - 1; i > 0; --i) {
        run.Move(poses[i], poses[i - 1], kTick, -1.0F);
    }
    if (!run.laps.IsWrongWay() || run.laps.CompletedLaps() != lapsBefore + kLaps ||
        run.laps.Progress(*graph) > progress - 0.5F) {
        problems.push_back(
            std::format("{}: driving a round backward didn't lose progress and warn", track));
    }
    return problems;
}

TEST_CASE("the pace car counts a lap each time round, and backwards warns of the wrong way") {
    const std::optional<assets::Vfs> vfs = RetailVfs();
    if (!vfs) {
        return;
    }

    const std::vector<std::string> tracks = assets::ListRaceTracks(*vfs);
    CHECK(tracks.size() == 13);
    for (const std::string& track : tracks) {
        for (const std::string& problem : PaceCarProblems(*vfs, track)) {
            FAIL_CHECK(problem);
        }
    }
}
}
}
