#include "app/replay_session.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "platform/mapped_file.hpp"
#include "sim/replay/snapshot_hash.hpp"

namespace rr::app {
namespace {

Result<void> WriteBytes(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), std::ssize(bytes));
    if (!file) {
        return Fail(ErrorCode::Io, std::format("can't write {}", Utf8FromPath(path)));
    }
    return {};
}

std::string Joined(std::span<const std::string> entries) {
    std::string joined;
    for (const std::string& entry : entries) {
        joined += joined.empty() ? entry : ", " + entry;
    }
    return joined;
}

std::vector<std::uint64_t> LoadHashes(const std::filesystem::path& replayFile) {
    const std::filesystem::path path = SnapshotHashesPath(replayFile);
    Result<std::vector<std::uint64_t>> hashes = platform::MappedFile::Open(path).and_then(
        [](const platform::MappedFile& file) { return sim::DecodeSnapshotHashes(file.Bytes()); });
    if (!hashes) {
        log::Warn("replay won't be checked: {}", hashes.error().message);
        return {};
    }
    return std::move(*hashes);
}

}

std::filesystem::path SnapshotHashesPath(const std::filesystem::path& replayFile) {
    std::filesystem::path path = replayFile;
    path += ".hashes";
    return path;
}

ReplayRecording::ReplayRecording(std::filesystem::path file, std::string scene, std::uint64_t seed,
                                 std::vector<std::string> gameplay)
    : file_(std::move(file)), recorder_(RR_VERSION, std::move(scene), seed, std::move(gameplay)) {}

void ReplayRecording::AfterTick(const sim::InputFrame& input, const sim::RenderSnapshot& after) {
    if (scenesStarted_ == 1) {
        recorder_.Record(input, after);
    }
}

Result<void> ReplayRecording::Save() const {
    Result<void> saved = WriteBytes(file_, sim::EncodeReplay(recorder_.Recorded())).and_then([&] {
        return WriteBytes(SnapshotHashesPath(file_), sim::EncodeSnapshotHashes(recorder_.Hashes()));
    });
    if (saved) {
        log::Info("replay: recorded {} ticks to {}", recorder_.Hashes().size(),
                  Utf8FromPath(file_));
    }
    return saved;
}

Result<ReplayPlayback> ReplayPlayback::Load(const std::filesystem::path& file,
                                            std::span<const std::string> gameplay) {
    Result<sim::Replay> replay = platform::MappedFile::Open(file).and_then(
        [](const platform::MappedFile& mapped) { return sim::DecodeReplay(mapped.Bytes()); });
    if (!replay) {
        return Fail(replay.error().code,
                    std::format("can't play {}: {}", Utf8FromPath(file), replay.error().message));
    }
    if (replay->buildVersion != RR_VERSION) {
        log::Warn("replay was recorded by version {}, this is {}", replay->buildVersion,
                  RR_VERSION);
    }
    if (!std::ranges::equal(replay->gameplay, gameplay)) {
        log::Warn(
            "replay was recorded with gameplay options [{}], this run has [{}], so it may drift",
            Joined(replay->gameplay), Joined(gameplay));
    }
    return ReplayPlayback(sim::ReplayPlayer(std::move(*replay), LoadHashes(file)));
}

std::optional<std::string> ReplayPlayback::Scene() const {
    const std::string& scene = player_.Playing().scene;
    return scene.empty() ? std::nullopt : std::optional<std::string>(scene);
}

void ReplayPlayback::BeforeTick(sim::InputFrame& input) {
    isReplayingTick_ = scenesStarted_ == 1 && !player_.IsFinished();
    if (isReplayingTick_) {
        input = player_.Next();
        if (player_.IsFinished()) {
            log::Info("replay: finished, control is back with the player");
        }
    }
}

void ReplayPlayback::AfterTick([[maybe_unused]] const sim::InputFrame& input,
                               const sim::RenderSnapshot& after) {
    if (isReplayingTick_) {
        player_.Check(after);
    }
}

void ReplayPlayback::LogOutcome() const {
    if (const std::optional<std::uint64_t> tick = player_.FirstMismatch()) {
        log::Error("replay: tick {} differs from the recording", *tick);
    } else {
        log::Info("replay: {} ticks checked, all match the recording", player_.CheckedTicks());
    }
}

Result<ReplaySession> ReplaySession::Start(const std::optional<std::filesystem::path>& recordFile,
                                           const std::optional<std::filesystem::path>& replayFile,
                                           std::optional<std::string>& scene,
                                           const std::vector<std::string>& gameplay) {
    ReplaySession session;
    if (recordFile && replayFile) {
        return Fail(ErrorCode::InvalidData, "--record and --replay can't be used together");
    }
    if (replayFile) {
        Result<ReplayPlayback> playback = ReplayPlayback::Load(*replayFile, gameplay);
        if (!playback) {
            return std::unexpected(std::move(playback.error()));
        }
        scene = playback->Scene();
        session.seed_ = playback->Player().Playing().seed;
        session.mode_.emplace<ReplayPlayback>(std::move(*playback));
        return session;
    }
    std::random_device random;
    session.seed_ = (std::uint64_t{random()} << 32U) | random();
    if (recordFile) {
        session.mode_.emplace<ReplayRecording>(*recordFile, scene.value_or(""), session.seed_,
                                               gameplay);
    }
    return session;
}

TickTap* ReplaySession::Tap() {
    return std::visit(
        []<typename Mode>(Mode& mode) -> TickTap* {
            if constexpr (std::same_as<Mode, std::monostate>) {
                return nullptr;
            } else {
                return &mode;
            }
        },
        mode_);
}

void ReplaySession::Finish() const {
    if (const auto* recording = std::get_if<ReplayRecording>(&mode_)) {
        const Result<void> saved = recording->Save();
        if (!saved) {
            log::Error("{}", saved.error().message);
        }
    } else if (const auto* playback = std::get_if<ReplayPlayback>(&mode_)) {
        playback->LogOutcome();
    }
}

}
