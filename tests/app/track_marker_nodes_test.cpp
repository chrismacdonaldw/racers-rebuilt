#include "app/track_marker_nodes.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/brick_layout.hpp"
#include "assets/checkpoints.hpp"
#include "assets/hazard_list.hpp"
#include "assets/start_grid.hpp"
#include "assets/track_loader.hpp"
#include "assets/track_markers.hpp"

namespace rr::app {
namespace {

constexpr float kTolerance = 1.0e-3F;

bool IsNear(const Vector3& a, const Vector3& b) {
    return Length(a - b) < kTolerance;
}

template <typename Asset>
Asset ReadRetail(const assets::Vfs& vfs, const std::string& path,
                 Result<Asset> (*parse)(std::span<const std::byte>)) {
    Result<Asset> asset =
        vfs.Read(path).and_then([&](const assets::FileData& file) { return parse(file.Bytes()); });
    REQUIRE(asset.has_value());
    return std::move(*asset);
}

assets::TrackMarkers RoundTrip(const assets::Vfs& vfs, const std::string& track) {
    assets::GltfScene scene;
    const Result<std::vector<std::size_t>> roots = AddTrackMarkers(vfs, track, scene);
    REQUIRE(roots.has_value());
    Result<assets::TrackMarkers> markers = assets::ParseTrackMarkers(scene.Build(*roots));
    REQUIRE(markers.has_value());
    return std::move(*markers);
}

std::vector<std::string> GridDifferences(const assets::StartGridAsset& read,
                                         const assets::StartGridAsset& original) {
    std::vector<std::string> differences;
    if (read.slots.size() != original.slots.size()) {
        return {"slot count"};
    }
    for (std::size_t i = 0; i < read.slots.size(); ++i) {
        const assets::StartSlot& a = read.slots[i];
        const assets::StartSlot& b = original.slots[i];
        if (!IsNear(a.position, b.position) ||
            !IsNear(a.forward, Normalized(b.forward, {.x = 1.0F}))) {
            differences.push_back(std::format("start.{}", i + 1));
        }
    }
    return differences;
}

std::vector<std::string> GateDifferences(const assets::CheckpointsAsset& read,
                                         const assets::CheckpointsAsset& original) {
    std::vector<std::string> differences;
    if (read.checkpoints.size() != original.checkpoints.size()) {
        return {"gate count"};
    }
    for (std::size_t i = 0; i < read.checkpoints.size(); ++i) {
        const assets::Checkpoint& a = read.checkpoints[i];
        const assets::Checkpoint& b = original.checkpoints[i];
        const bool isSame = IsNear(a.position, b.position) && a.distance == b.distance &&
                            a.links == b.links &&
                            IsNear(a.normal, Normalized(b.normal, {.x = -1.0F}));
        if (!isSame) {
            differences.push_back(std::format("gate.{:02}", i + 1));
        }
    }
    return differences;
}

std::vector<std::string> BrickDifferences(const std::optional<assets::BrickLayout>& read,
                                          const assets::BrickLayout& original) {
    if (!read) {
        return original.coloured.empty() && original.white.empty()
                   ? std::vector<std::string>{}
                   : std::vector<std::string>{"bricks missing"};
    }
    std::vector<std::string> differences;
    if (read->coloured.size() != original.coloured.size() || read->white != original.white) {
        return {"brick count"};
    }
    for (std::size_t i = 0; i < read->coloured.size(); ++i) {
        const assets::ColourBrickPlacement& a = read->coloured[i];
        const assets::ColourBrickPlacement& b = original.coloured[i];
        if (a.position != b.position || a.colour != b.colour || a.respawn != b.respawn) {
            differences.push_back(std::format("brick {}", i));
        }
    }
    return differences;
}

std::vector<std::string> RoundTripDifferences(const assets::Vfs& vfs, const std::string& track) {
    const assets::TrackMarkers markers = RoundTrip(vfs, track);
    const std::string folder = std::format("GAMEDATA/{}/", track);
    std::vector<std::string> differences = markers.problems;
    REQUIRE(markers.grid.has_value());
    REQUIRE(markers.gates.has_value());
    for (std::vector<std::string> found :
         {GridDifferences(*markers.grid,
                          ReadRetail(vfs, folder + "STARTPOS.SPB", assets::ParseSpb)),
          GateDifferences(*markers.gates, ReadRetail(vfs, folder + "RACE.CPB", assets::ParseCpb)),
          BrickDifferences(markers.bricks,
                           ReadRetail(vfs, folder + "POWERUP.PWB", assets::ParsePwb))}) {
        differences.insert(differences.end(), found.begin(), found.end());
    }
    if (vfs.Contains(folder + "HAZARDS.HZB") &&
        markers.hazards.value_or(std::vector<assets::Hazard>{}) !=
            ReadRetail(vfs, folder + "HAZARDS.HZB", assets::ParseHazards)) {
        differences.emplace_back("hazards");
    }
    return differences;
}

bool IsSameRoute(const assets::RaceRoute& read, const assets::RaceRoute& original) {
    return read.points.size() == original.points.size() &&
           read.points.back().durationMs == original.points.back().durationMs &&
           read.loopTimeMs == original.loopTimeMs &&
           IsNear(read.startPosition, original.startPosition);
}

TEST_CASE("every retail track's markers read back from glTF as its own files") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    for (const std::string& track : assets::ListRaceTracks(*vfs)) {
        CAPTURE(track);
        std::string differences;
        for (const std::string& difference : RoundTripDifferences(*vfs, track)) {
            differences += difference + "; ";
        }
        CHECK(differences == "");
    }
}

TEST_CASE("retail routes read back with their points and timing") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const assets::RaceRoute original =
        ReadRetail(*vfs, "GAMEDATA/RACEC0R0/R1_F_0.RRB", assets::ParseRaceRoute);

    const assets::TrackMarkers markers = RoundTrip(*vfs, "RACEC0R0");

    REQUIRE_FALSE(markers.routes.empty());
    CHECK(markers.routes.front().slot == 1);
    CHECK(IsSameRoute(markers.routes.front().route, original));
}

}
}
