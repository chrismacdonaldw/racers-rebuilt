#include <SDL3/SDL_stdinc.h>
#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/track_loader.hpp"
#include "frame_times.hpp"
#include "race_drive.hpp"
#include "sim/collision/world_list.hpp"
#include "sim/race/race_track.hpp"
#include "sim/replay/replay.hpp"
#include "sim/replay/snapshot_hash.hpp"
#include "test_track.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kTestTrackRace{40'000};
constexpr Milliseconds kRetailRace{150'000};
constexpr std::uint64_t kRetailSeed = 11;

std::filesystem::path RecordedReplay() {
    return std::filesystem::path(RR_TEST_DATA_DIR) / "replays" / "test_track.replay";
}

std::vector<Nanoseconds> FramesFor(int fps, Milliseconds duration) {
    return fps == 0 ? test::UncappedFrames(duration) : test::FixedRateFrames(fps, duration);
}

void CheckEveryFramerate(const sim::Replay& replay, const std::vector<std::uint64_t>& expected,
                         const sim::RaceSetup& setup, const sim::RaceCourse& course,
                         Milliseconds duration) {
    for (const int fps : {30, 60, 144}) {
        CAPTURE(fps);
        const test::DriveResult run =
            test::Drive(replay, expected, setup, course, FramesFor(fps, duration));
        CHECK(run.checkedTicks == expected.size());
        CHECK_FALSE(run.firstMismatch.has_value());
    }
}

std::vector<std::byte> ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::vector<char> chars{std::istreambuf_iterator<char>(file), {}};
    std::vector<std::byte> bytes(chars.size());
    std::ranges::transform(chars, bytes.begin(), [](char c) { return static_cast<std::byte>(c); });
    return bytes;
}

void WriteFile(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary | std::ios::trunc)
        .write(reinterpret_cast<const char*>(bytes.data()), std::ssize(bytes));
}

std::filesystem::path HashesPath(const std::filesystem::path& replay) {
    std::filesystem::path path = replay;
    path += ".hashes";
    return path;
}

TEST_CASE("a scripted race on the test track replays identically at every framerate") {
    const test::TestTrack track;
    const sim::Replay replay = test::ScriptedReplay(kTestTrackRace);
    const sim::RaceSetup setup = test::TestTrack::Setup(replay.seed);

    const test::DriveResult uncapped =
        test::Drive(replay, {}, setup, track.Course(), FramesFor(0, kTestTrackRace));

    REQUIRE(uncapped.hashes.size() == replay.racers.front().size());
    CHECK(uncapped.respawns > 0);
    CHECK(uncapped.mostLaps > 0);
    CheckEveryFramerate(replay, uncapped.hashes, setup, track.Course(), kTestTrackRace);
}

void RecordTestTrackRace(const test::TestTrack& track) {
    const sim::Replay replay = test::ScriptedReplay(kTestTrackRace);
    const test::DriveResult run = test::Drive(replay, {}, test::TestTrack::Setup(replay.seed),
                                              track.Course(), FramesFor(0, kTestTrackRace));
    WriteFile(RecordedReplay(), sim::EncodeReplay(replay));
    WriteFile(HashesPath(RecordedReplay()), sim::EncodeSnapshotHashes(run.hashes));
}

struct Playback {
    test::DriveResult run;
    std::size_t recordedTicks = 0;
};

Playback PlayRecordedRace(const test::TestTrack& track) {
    const Result<sim::Replay> replay = sim::DecodeReplay(ReadFile(RecordedReplay()));
    const Result<std::vector<std::uint64_t>> hashes =
        sim::DecodeSnapshotHashes(ReadFile(HashesPath(RecordedReplay())));
    REQUIRE(replay.has_value());
    REQUIRE(hashes.has_value());
    CHECK(replay->scene == "test-track");
    return {.run = test::Drive(*replay, *hashes, test::TestTrack::Setup(replay->seed),
                               track.Course(), FramesFor(60, kTestTrackRace)),
            .recordedTicks = hashes->size()};
}

TEST_CASE("the recorded test track race still plays back tick for tick") {
    const test::TestTrack track;
    if (SDL_getenv("RR_RECORD_TEST_REPLAY") != nullptr) {
        RecordTestTrackRace(track);
    }

    const Playback playback = PlayRecordedRace(track);

    CHECK(playback.recordedTicks > 0);
    CHECK(playback.run.checkedTicks == playback.recordedTicks);
    CHECK_FALSE(playback.run.firstMismatch.has_value());
}

void CheckRetailTrack(const assets::Vfs& vfs, const std::string& name, const sim::Replay& replay) {
    CAPTURE(name);
    const Result<sim::RaceTrack> track = sim::LoadRaceTrack(vfs, name);
    REQUIRE(track.has_value());
    sim::RaceSetup setup{.track = name, .seed = kRetailSeed};
    setup.racers.resize(track->grid.size());
    setup.racers.front().isPlayer = true;
    const sim::WorldList world(track->collision.Track());
    const sim::RaceCourse course{.checkpoints = &track->checkpoints,
                                 .grid = track->grid,
                                 .world = &world,
                                 .lowestTrackZ = track->lowestTrackZ,
                                 .routes = &track->routes};

    const test::DriveResult uncapped =
        test::Drive(replay, {}, setup, course, FramesFor(0, kRetailRace));

    REQUIRE(uncapped.hashes.size() == replay.racers.front().size());
    CHECK(uncapped.mostLaps > 0);
    CheckEveryFramerate(replay, uncapped.hashes, setup, course, kRetailRace);
}

TEST_CASE("a full grid races identically at every framerate on every retail track") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const sim::Replay replay = test::ScriptedReplay(kRetailRace);
    for (const std::string& name : assets::ListRaceTracks(*vfs)) {
        CheckRetailTrack(*vfs, name, replay);
    }
}

}
}
