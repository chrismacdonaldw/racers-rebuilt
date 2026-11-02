#pragma once

#include <optional>
#include <span>

#include "assets/ghost_run.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"
#include "sim/time_trial/ghost_playback.hpp"
#include "sim/time_trial/ghost_recorder.hpp"

namespace rr::sim {

inline constexpr Milliseconds kGhostDrawLimit{240000};

class TimeTrialGhosts {
public:
    /// A record without samples counts as no record.
    explicit TimeTrialGhosts(std::optional<assets::GhostRun> record);

    /// Call at the go.
    void Start();

    /// Call once per tick from the go until the finish, after the racers move. `playerPose` is in
    /// game space.
    void Tick(Milliseconds elapsed, const Transform& playerPose);

    /// Stops both ghosts' clock. The run becomes the best when its race time is lower.
    void Finish(const assets::GhostLapTimes& laps);

    /// Drops the run in progress and sends both ghosts back to their start.
    void Restart();

    /// Whether the best run's race time is strictly lower than the record's.
    [[nodiscard]] bool HasBeatenRecord() const;

    /// How far the player's completed laps trail the ghost to beat, or 0 when they don't.
    [[nodiscard]] Milliseconds TimeBehind(std::span<const Milliseconds> completedLaps) const;

    /// Nothing while the ghost isn't drawn.
    [[nodiscard]] std::optional<GhostFrame> RecordFrame() const;
    [[nodiscard]] std::optional<GhostFrame> BestFrame() const;

private:
    [[nodiscard]] std::optional<GhostFrame> FrameOf(const assets::GhostRun& run) const;
    [[nodiscard]] bool HasRunOut(const std::optional<assets::GhostRun>& run) const;

    std::optional<assets::GhostRun> record_;
    std::optional<assets::GhostRun> best_;
    GhostRecorder recorder_;
    Milliseconds time_{0};
    bool isRunning_ = false;
    bool isBestShown_ = false;
    bool isRecordDone_ = false;
    bool isBestDone_ = false;
};

}
