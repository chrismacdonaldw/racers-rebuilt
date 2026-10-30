#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
#include <format>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/race_route.hpp"
#include "assets/track_loader.hpp"
#include "sim/collision/track_collision.hpp"
#include "sim/collision/world_list.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr int kTicksPerRun = 150;
constexpr std::size_t kPointStride = 20;
constexpr float kTopSpeed = 0.12F;
constexpr float kMaxWallNormalZ = 0.7F;
constexpr Vector3 kAboveRoute{.z = 2.0F};

struct WallRuns {
    int hits = 0;
    int crossings = 0;
};

struct Launch {
    Vector3 position;
    float headingRadians = 0.0F;
};

std::optional<assets::RaceRoute> FirstRoute(const assets::Vfs& vfs, const std::string& folder) {
    const std::string directory = std::format("GAMEDATA/{}", folder);
    for (const assets::VfsEntry& entry :
         vfs.List(directory).value_or(std::vector<assets::VfsEntry>{})) {
        if (!entry.name.ends_with(".RRB")) {
            continue;
        }
        Result<assets::RaceRoute> route = vfs.Read(std::format("{}/{}", directory, entry.name))
                                              .and_then([](const assets::FileData& data) {
                                                  return assets::ParseRaceRoute(data.Bytes());
                                              });
        if (route) {
            return std::move(*route);
        }
    }
    return std::nullopt;
}

std::vector<Launch> SidewaysLaunches(const assets::RaceRoute& route) {
    std::vector<Launch> launches;
    Vector3 position = route.startPosition;
    for (std::size_t i = 0; i + 1 < route.points.size(); ++i) {
        position += route.points[i].offset;
        if (i % kPointStride != 0) {
            continue;
        }
        const Vector3& ahead = route.points[i + 1].offset;
        const float heading = std::atan2(ahead.y, ahead.x);
        for (const float side : {-1.0F, 1.0F}) {
            launches.push_back(
                {.position = position + kAboveRoute,
                 .headingRadians = heading + (side * std::numbers::pi_v<float> / 2.0F)});
        }
    }
    return launches;
}

int WallCrossings(const BodyConstants& constants, const BodyMotion& before, const BodyMotion& after,
                  const CollisionQuery& world) {
    int crossings = 0;
    for (const Vector3& point : constants.bodyPoints) {
        const Vector3 start = before.position + ToWorld(before.basis, point);
        const Vector3 end = after.position + ToWorld(after.basis, point);
        for (std::size_t i = 0; i < world.WorldCount(); ++i) {
            const std::optional<SegmentHit> hit = world.CastSegment(i, start, end);
            if (hit && hit->isSolid && std::abs(hit->normal.z) < kMaxWallNormalZ &&
                Dot(end - hit->point, hit->normal) < 0.0F) {
                ++crossings;
            }
        }
    }
    return crossings;
}

void RunIntoWalls(const Launch& launch, const CollisionQuery& world, WallRuns& runs) {
    Racer racer({}, {}, {.position = launch.position, .headingRadians = launch.headingRadians});
    const ContactBody body = racer.ForContacts(world);
    body.body->motion.velocity = body.body->motion.basis.forward * kTopSpeed;
    bool hasHit = false;
    for (int tick = 0; tick < kTicksPerRun; ++tick) {
        const BodyMotion before = racer.Body().motion;
        racer.Tick(kTick, {.isThrottleHeld = true}, world);
        runs.crossings += WallCrossings(*body.constants, before, racer.Body().motion, world);
        hasHit = hasHit || racer.Body().hasWallContact;
    }
    runs.hits += hasHit ? 1 : 0;
}

void CheckTrack(const assets::Vfs& vfs, const std::string& folder) {
    const Result<TrackCollision> collision = LoadTrackCollision(vfs, folder);
    const std::optional<assets::RaceRoute> route = FirstRoute(vfs, folder);
    if (!collision || !route) {
        FAIL_CHECK(folder);
        return;
    }
    const WorldList world(collision->Track());
    WallRuns runs;
    const std::vector<Launch> launches = SidewaysLaunches(*route);
    for (const Launch& launch : launches) {
        RunIntoWalls(launch, world, runs);
    }
    MESSAGE(std::format("{}: {} launches, {} hit a wall, {} wall crossings", folder,
                        launches.size(), runs.hits, runs.crossings));
    CHECK(runs.hits > 0);
    CHECK(runs.crossings == 0);
}

TEST_CASE("cars driven sideways into retail walls at top speed never pass through them") {
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
