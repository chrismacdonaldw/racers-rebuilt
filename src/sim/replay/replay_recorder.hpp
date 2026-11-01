#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "sim/input_frame.hpp"
#include "sim/render_snapshot.hpp"
#include "sim/replay/replay.hpp"

namespace rr::sim {

/// Records one human racer.
class ReplayRecorder {
public:
    ReplayRecorder(std::string buildVersion, std::string scene, std::uint64_t seed,
                   std::vector<std::string> gameplay = {});

    void Record(const InputFrame& input, const RenderSnapshot& after);

    [[nodiscard]] const Replay& Recorded() const { return replay_; }
    [[nodiscard]] std::span<const std::uint64_t> Hashes() const { return hashes_; }

private:
    Replay replay_;
    std::vector<std::uint64_t> hashes_;
};

/// Plays back the first racer, checking each tick against the recorded hashes.
class ReplayPlayer {
public:
    ReplayPlayer(Replay replay, std::vector<std::uint64_t> hashes);

    [[nodiscard]] const Replay& Playing() const { return replay_; }
    [[nodiscard]] bool IsFinished() const;

    /// Call once per tick, before `Check`, until `IsFinished`.
    [[nodiscard]] const InputFrame& Next();
    void Check(const RenderSnapshot& after);

    [[nodiscard]] std::size_t CheckedTicks() const { return checked_; }
    /// Counted from 1.
    [[nodiscard]] std::optional<std::uint64_t> FirstMismatch() const { return firstMismatch_; }

private:
    Replay replay_;
    std::vector<std::uint64_t> hashes_;
    std::size_t next_ = 0;
    std::size_t checked_ = 0;
    std::optional<std::uint64_t> firstMismatch_;
};

}
