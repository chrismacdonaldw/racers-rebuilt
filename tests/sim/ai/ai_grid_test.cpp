#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/track_loader.hpp"
#include "sim/ai/ai_racer.hpp"
#include "sim/ai/route_choice.hpp"
#include "sim/race/checkpoint_graph.hpp"
#include "sim/race/lap_tracker.hpp"
#include "sim/race/race_track.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr int kRaceLaps = 3;
constexpr int kGridSlots = 6;
constexpr double kLapTolerance = 0.02;

Milliseconds RecordedLap(const assets::RaceRoute& route) {
    const Milliseconds length{
        std::accumulate(route.points.begin(), route.points.end(), std::int64_t{0},
                        [](std::int64_t total, const assets::RoutePoint& point) {
                            return total + point.durationMs;
                        })};
    return length - Milliseconds{route.loopTimeMs};
}

std::vector<Milliseconds> LapFinishes(const assets::RaceRoute& route, const CheckpointGraph& graph,
                                      int laps) {
    const Milliseconds limit = Milliseconds{route.loopTimeMs} + ((laps + 1) * RecordedLap(route));
    AiRacer racer(route);
    LapTracker tracker;
    std::vector<Milliseconds> finishes;
    for (Milliseconds at{0}; at < limit && std::cmp_less(finishes.size(), laps); at += kTick) {
        const Vector3 from = racer.Pose().position;
        racer.Tick(kTick);
        if (const auto crossing = graph.FindCrossing(from, racer.Pose().position)) {
            tracker.Cross(graph, *crossing);
        }
        tracker.Update(graph, kTick, racer.Facing());
        if (std::cmp_greater(tracker.CompletedLaps(), finishes.size())) {
            finishes.push_back(at + kTick);
        }
    }
    return finishes;
}

struct Track {
    std::string name;
    CheckpointGraph graph;
    std::vector<assets::VfsEntry> folder;
};

using TrackCheck = std::vector<std::string> (*)(const assets::Vfs&, const Track&);

void CheckEveryTrack(TrackCheck check) {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    for (const std::string& name : assets::ListRaceTracks(*vfs)) {
        Result<CheckpointGraph> graph = LoadCheckpoints(*vfs, name);
        Result<std::vector<assets::VfsEntry>> folder = vfs->List("GAMEDATA/" + name);
        std::vector<std::string> problems;
        if (graph && folder) {
            problems = check(
                *vfs, {.name = name, .graph = std::move(*graph), .folder = std::move(*folder)});
        } else {
            problems.push_back(
                std::format("{}: {}", name, (graph ? folder.error() : graph.error()).message));
        }
        for (const std::string& problem : problems) {
            FAIL_CHECK(problem);
        }
    }
}

std::vector<std::string> FastLapProblems(const assets::Vfs& vfs, const Track& track) {
    const Result<assets::RaceRoute> route = LoadRoute(vfs, track.name, "R1_F_0.RRB", false);
    if (!route) {
        return {std::format("{}: {}", track.name, route.error().message)};
    }
    const std::vector<Milliseconds> finishes = LapFinishes(*route, track.graph, kRaceLaps);
    if (finishes.size() != kRaceLaps) {
        return {std::format("{}: {} laps", track.name, finishes.size())};
    }
    const auto recorded = static_cast<double>(RecordedLap(*route).count());
    const auto lap = static_cast<double>((finishes[2] - finishes[1]).count());
    if (std::abs(lap - recorded) > kLapTolerance * recorded) {
        return {std::format("{}: lap of {} ms, recorded {} ms", track.name, lap, recorded)};
    }
    return {};
}

std::vector<std::string> GridProblems(const assets::Vfs& vfs, const Track& track) {
    std::vector<std::string> problems;
    for (int slot = 0; slot < kGridSlots; ++slot) {
        for (const std::string& name : SlotRoutes(track.folder, slot)) {
            const Result<assets::RaceRoute> route = LoadRoute(vfs, track.name, name, false);
            const std::size_t laps = route ? LapFinishes(*route, track.graph, kRaceLaps).size() : 0;
            if (laps != kRaceLaps) {
                problems.push_back(std::format("{} {}: {} laps", track.name, name, laps));
            }
        }
    }
    return problems;
}

TEST_CASE("an ai racer on R1_F_0 laps every track within 2% of the recorded lap") {
    CheckEveryTrack(FastLapProblems);
}

TEST_CASE("every route a grid slot can draw finishes three laps on every track") {
    CheckEveryTrack(GridProblems);
}

}
}
