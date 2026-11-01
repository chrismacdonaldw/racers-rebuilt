#include "sim/race/race_track.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/track_loader.hpp"
#include "sim/collision/world_list.hpp"
#include "sim/powerup/brick_field.hpp"
#include "sim/powerup/green_effects.hpp"
#include "sim/race/race_session.hpp"
#include "sim/race/respawn.hpp"

namespace rr::sim {
namespace {

constexpr float kMaxHeightOffGround = 8.0F;
constexpr float kOffTrackZ = 10000.0F;
constexpr Milliseconds kTick{12};
constexpr int kDriveTicks = 750;
constexpr Vector3 kAboveBrick{.z = 10.0F};
constexpr Vector3 kBelowBrick{.z = -50.0F};
constexpr float kMaxBrickHeight = 8.0F;
constexpr int kWarpTicks = 167;
constexpr int kSettleTicks = 84;
constexpr float kMinWarpGain = 0.1F;
constexpr float kLandingHeight = 5.0F;

bool IsGoodSlot(const RaceTrack& track, const WorldList& world, const RacerStart& slot) {
    const RacerStart dropped = RespawnPoint(track.checkpoints, std::nullopt, slot, world);
    return SignedDistance(track.checkpoints.Checkpoints().front(), slot.position) > 0.0F &&
           std::abs(slot.position.z - dropped.position.z) < kMaxHeightOffGround &&
           dropped.position.z >= track.lowestTrackZ;
}

bool HasGroundToRespawnOn(const RaceTrack& track, const WorldList& world, std::size_t checkpoint) {
    const RacerStart point =
        RespawnPoint(track.checkpoints, checkpoint, {.position = {.z = kOffTrackZ}}, world);
    return point.position.z >= track.lowestTrackZ &&
           point.position.z <= track.checkpoints.Checkpoints()[checkpoint].position.z + 2.0F;
}

std::vector<std::size_t> BadSlots(const RaceTrack& track, const WorldList& world) {
    std::vector<std::size_t> bad;
    for (std::size_t slot = 0; slot < track.grid.size(); ++slot) {
        if (!IsGoodSlot(track, world, track.grid[slot])) {
            bad.push_back(slot);
        }
    }
    return bad;
}

std::vector<std::size_t> CheckpointsWithoutGround(const RaceTrack& track, const WorldList& world) {
    std::vector<std::size_t> bad;
    for (std::size_t checkpoint = 0; checkpoint < track.checkpoints.Checkpoints().size();
         ++checkpoint) {
        if (!HasGroundToRespawnOn(track, world, checkpoint)) {
            bad.push_back(checkpoint);
        }
    }
    return bad;
}

void CheckTrack(const assets::Vfs& vfs, const std::string& folder) {
    CAPTURE(folder);
    const Result<RaceTrack> track = LoadRaceTrack(vfs, folder);
    if (!track) {
        FAIL(track.error().message);
    }
    const WorldList world(track->collision.Track());
    CHECK(track->grid.size() == 6);
    CHECK(BadSlots(*track, world).empty());
    CHECK(CheckpointsWithoutGround(*track, world).empty());
}

struct DriveCheck {
    bool hasCrossedStart = false;
    int respawns = 0;
};

DriveCheck DriveFromGrid(const RaceTrack& track) {
    const WorldList world(track.collision.Track());
    Result<RaceSession> session = RaceSession::Create(
        {.racers = std::vector<RacerEntry>(track.grid.size(), {.isPlayer = true})},
        {.checkpoints = &track.checkpoints,
         .grid = track.grid,
         .world = &world,
         .lowestTrackZ = track.lowestTrackZ});
    REQUIRE(session.has_value());
    const std::vector<RacerControls> throttle(track.grid.size(), {.isThrottleHeld = true});
    DriveCheck check;
    for (int tick = 0; tick < kDriveTicks; ++tick) {
        session->Tick(kTick, throttle);
        check.respawns += session->WasRespawned(0) ? 1 : 0;
    }
    check.hasCrossedStart = session->Progress().OfRacer(0).laps.LastCheckpoint().has_value();
    return check;
}

void CheckDrive(const assets::Vfs& vfs, const std::string& folder) {
    CAPTURE(folder);
    const Result<RaceTrack> track = LoadRaceTrack(vfs, folder);
    if (!track) {
        FAIL(track.error().message);
    }
    const DriveCheck check = DriveFromGrid(*track);
    CHECK(check.respawns == 0);
    CHECK(check.hasCrossedStart);
}

struct WarpCheck {
    float progressGain = 0.0F;
    float heightOffGround = kOffTrackZ;
    int respawns = 0;
};

WarpCheck WarpFromGrid(const RaceTrack& track) {
    const WorldList world(track.collision.Track());
    RaceSetup setup{.racers = std::vector<RacerEntry>(track.grid.size(), {.isPlayer = true})};
    AddGreenEffects(setup.effects);
    Result<RaceSession> session = RaceSession::Create(setup, {.checkpoints = &track.checkpoints,
                                                              .grid = track.grid,
                                                              .world = &world,
                                                              .lowestTrackZ = track.lowestTrackZ});
    REQUIRE(session.has_value());
    std::vector<RacerControls> controls(track.grid.size(), {.isThrottleHeld = true});
    for (int tick = 0; tick < kDriveTicks; ++tick) {
        session->Tick(kTick, controls);
    }
    const RacerProgress& progress = session->Progress().OfRacer(0);
    const float before = progress.laps.Progress(track.checkpoints);
    session->GivePowerup(0, assets::BrickColour::Green, 3);
    controls[0].isUsePressed = true;
    session->Tick(kTick, controls);
    controls[0].isUsePressed = false;
    WarpCheck check;
    for (int tick = 0; tick < kWarpTicks + kSettleTicks; ++tick) {
        session->Tick(kTick, controls);
        check.respawns += session->WasRespawned(0) ? 1 : 0;
        const Racer& racer = session->Racers()[0];
        const Vector3 at = racer.Body().motion.position;
        const std::optional<SegmentHit> ground = CastSolid(world, at, at + kBelowBrick);
        if (racer.Events().hasWarpLanded && ground) {
            check.heightOffGround = at.z - ground->point.z;
        }
    }
    check.progressGain = progress.laps.Progress(track.checkpoints) - before;
    return check;
}

void CheckWarp(const assets::Vfs& vfs, const std::string& folder) {
    CAPTURE(folder);
    const Result<RaceTrack> track = LoadRaceTrack(vfs, folder);
    if (!track) {
        FAIL(track.error().message);
    }
    const WarpCheck check = WarpFromGrid(*track);
    MESSAGE(folder << ": a warp gains " << check.progressGain << " laps");
    CHECK(check.progressGain > kMinWarpGain);
    CHECK(check.heightOffGround == doctest::Approx(kLandingHeight));
    CHECK(check.respawns == 0);
}

struct BrickCheck {
    std::size_t bricks = 0;
    std::size_t reached = 0;
    std::vector<Vector3> tooHigh;
};

bool CarOnGroundTakes(const Vector3& ground, const Vector3& brick) {
    const BoundingSphere reach{.centre = ground + kTypicalRacerBounds.centre,
                               .radius = kTypicalRacerBounds.radius};
    BrickField field({.white = {brick}});
    std::array<PowerupInventory, 1> held{};
    field.Tick(kTick, std::span(&reach, 1), held);
    return held[0].WhiteCount() == 1;
}

void CheckBrick(const WorldList& world, const Vector3& brick, BrickCheck& check) {
    const std::optional<SegmentHit> ground =
        CastSolid(world, brick + kAboveBrick, brick + kBelowBrick);
    if (!ground) {
        return;
    }
    ++check.bricks;
    if (brick.z - ground->point.z > kMaxBrickHeight) {
        check.tooHigh.push_back(brick);
    }
    check.reached += CarOnGroundTakes(ground->point, brick) ? 1 : 0;
}

BrickCheck CheckBricks(const assets::Vfs& vfs, const std::string& folder) {
    const Result<RaceTrack> track = LoadRaceTrack(vfs, folder);
    if (!track) {
        FAIL(track.error().message);
    }
    const WorldList world(track->collision.Track());
    BrickCheck check;
    for (const assets::ColourBrickPlacement& brick : track->bricks.coloured) {
        CheckBrick(world, brick.position, check);
    }
    for (const Vector3& brick : track->bricks.white) {
        CheckBrick(world, brick, check);
    }
    return check;
}

TEST_CASE(
    "on every retail race track, bricks over the track float just above it and a car below takes "
    "them") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    for (const std::string& folder : assets::ListRaceTracks(*vfs)) {
        CAPTURE(folder);
        const BrickCheck check = CheckBricks(*vfs, folder);
        MESSAGE(folder << ": a stock car below takes " << check.reached << " of " << check.bricks
                       << " bricks over the track");
        CHECK(check.tooHigh.empty());
        CHECK(check.reached == check.bricks);
    }
}

TEST_CASE("on every retail race track, the pole racer drives off the grid without respawning") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    for (const std::string& folder : assets::ListRaceTracks(*vfs)) {
        CheckDrive(*vfs, folder);
    }
}

TEST_CASE("on every retail race track, a warp carries the pole racer ahead and onto the track") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    for (const std::string& folder : assets::ListRaceTracks(*vfs)) {
        CheckWarp(*vfs, folder);
    }
}

TEST_CASE(
    "every retail race track loads a grid of 6 behind checkpoint 0, near the ground, with ground "
    "under every respawn") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    for (const std::string& folder : assets::ListRaceTracks(*vfs)) {
        CheckTrack(*vfs, folder);
    }
}

TEST_CASE("a race track loads the target points in its TGB") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<RaceTrack> track = LoadRaceTrack(*vfs, "RACEC0R2");
    REQUIRE(track.has_value());
    REQUIRE(track->targets.size() == 2);
    CHECK(track->targets[1].position.x == doctest::Approx(-534.24F).epsilon(0.001));
    CHECK_FALSE(track->targets[1].removingEvent);
}

TEST_CASE("a race track without a TGB has no target points") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<RaceTrack> track = LoadRaceTrack(*vfs, "RACEC0R0");
    REQUIRE(track.has_value());
    CHECK(track->targets.empty());
}

}
}
