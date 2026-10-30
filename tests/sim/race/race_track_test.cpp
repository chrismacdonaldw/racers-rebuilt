#include "sim/race/race_track.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/track_loader.hpp"
#include "sim/collision/world_list.hpp"
#include "sim/race/respawn.hpp"

namespace rr::sim {
namespace {

constexpr float kMaxHeightOffGround = 8.0F;
constexpr float kOffTrackZ = 10000.0F;

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

}
}
