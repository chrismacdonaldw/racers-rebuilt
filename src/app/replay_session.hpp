#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "app/frame_loop.hpp"
#include "core/result.hpp"
#include "sim/replay/replay_recorder.hpp"

namespace rr::app {

/// Later scenes than the first aren't recorded.
class ReplayRecording final : public TickTap {
public:
    ReplayRecording(std::filesystem::path file, std::string scene, std::uint64_t seed);

    void SceneStarted() override { ++scenesStarted_; }
    void BeforeTick([[maybe_unused]] sim::InputFrame& input) override {}
    void AfterTick(const sim::InputFrame& input, const sim::RenderSnapshot& after) override;

    /// Writes the replay, and its snapshot hashes beside it with a `.hashes` suffix.
    [[nodiscard]] Result<void> Save() const;

private:
    std::filesystem::path file_;
    sim::ReplayRecorder recorder_;
    int scenesStarted_ = 0;
};

/// Drives the first scene it sees with the recorded input, then hands control back to the player.
class ReplayPlayback final : public TickTap {
public:
    /// Plays without checking if the `.hashes` file is missing.
    [[nodiscard]] static Result<ReplayPlayback> Load(const std::filesystem::path& file);

    [[nodiscard]] std::optional<std::string> Track() const;

    void SceneStarted() override { ++scenesStarted_; }
    void BeforeTick(sim::InputFrame& input) override;
    void AfterTick(const sim::InputFrame& input, const sim::RenderSnapshot& after) override;

    void LogOutcome() const;
    [[nodiscard]] const sim::ReplayPlayer& Player() const { return player_; }

private:
    explicit ReplayPlayback(sim::ReplayPlayer player) : player_(std::move(player)) {}

    sim::ReplayPlayer player_;
    int scenesStarted_ = 0;
    bool isReplayingTick_ = false;
};

class ReplaySession {
public:
    /// A replay names its own scene, so it replaces `track`.
    [[nodiscard]] static Result<ReplaySession> Start(
        const std::optional<std::filesystem::path>& recordFile,
        const std::optional<std::filesystem::path>& replayFile, std::optional<std::string>& track);

    /// Null when neither was asked for.
    [[nodiscard]] TickTap* Tap();
    void Finish() const;

private:
    std::variant<std::monostate, ReplayRecording, ReplayPlayback> mode_;
};

[[nodiscard]] std::filesystem::path SnapshotHashesPath(const std::filesystem::path& replayFile);

}
