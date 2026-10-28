#include "sim/collision/track_collision.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/race_route.hpp"
#include "assets/track_loader.hpp"

namespace rr::sim {
namespace {

constexpr Vector3 kAboveRoute{.z = 2.0F};
constexpr Vector3 kBelowJumps{.z = 200.0F};
constexpr std::string_view kTrackWithGap = "RACEC1R2";

struct RouteCheck {
    std::size_t points = 0;
    std::size_t misses = 0;
};

void CheckRoute(const CollisionWorld& track, const assets::RaceRoute& route, RouteCheck& check) {
    Vector3 position = route.startPosition;
    for (const assets::RoutePoint& point : route.points) {
        position += point.offset;
        ++check.points;
        const std::optional<SegmentHit> hit =
            track.CastSegment(position + kAboveRoute, position - kBelowJumps);
        if (!hit || !hit->isSolid || hit->normal.z <= 0.0F) {
            ++check.misses;
        }
    }
}

RouteCheck CheckRoutes(const assets::Vfs& vfs, const std::string& folder,
                       const CollisionWorld& track) {
    RouteCheck check;
    const std::string directory = std::format("GAMEDATA/{}", folder);
    for (const assets::VfsEntry& entry :
         vfs.List(directory).value_or(std::vector<assets::VfsEntry>{})) {
        if (!entry.name.ends_with(".RRB")) {
            continue;
        }
        const Result<assets::RaceRoute> route =
            vfs.Read(std::format("{}/{}", directory, entry.name))
                .and_then([](const assets::FileData& data) {
                    return assets::ParseRaceRoute(data.Bytes());
                });
        REQUIRE(route.has_value());
        CheckRoute(track, route.value(), check);
    }
    return check;
}

void CheckTrack(const assets::Vfs& vfs, const std::string& folder) {
    const Result<TrackCollision> collision = LoadTrackCollision(vfs, folder);
    if (!collision) {
        FAIL_CHECK(collision.error().message);
        return;
    }
    const RouteCheck routes = CheckRoutes(vfs, folder, collision->Track());
    MESSAGE(std::format("{}: {} worlds, {} route points, {} off the track", folder,
                        collision->worlds.size(), routes.points, routes.misses));
    CHECK(routes.points > 0);
    CHECK(routes.misses == (folder == kTrackWithGap ? 1U : 0U));
}

TEST_CASE("every retail track's collision loads and its race routes run on solid ground") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    for (const std::string& folder : assets::ListRaceTracks(*vfs)) {
        CheckTrack(*vfs, folder);
    }
}

}
}
