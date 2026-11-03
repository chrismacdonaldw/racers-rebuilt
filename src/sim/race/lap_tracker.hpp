#pragma once

#include <cstddef>
#include <optional>

#include "core/duration.hpp"
#include "core/vector3.hpp"
#include "sim/race/checkpoint_graph.hpp"

namespace rr::sim {

class LapTracker {
public:
    void Cross(const CheckpointGraph& graph, const GateCrossing& crossing);
    void ReachFinish(int lapCount);

    /// `facing` is the racer's facing direction in game space.
    void Update(const CheckpointGraph& graph, Milliseconds elapsed, const Vector3& facing);

    /// Checkpoint laps plus the lap fraction of the last checkpoint crossed.
    [[nodiscard]] float Progress(const CheckpointGraph& graph) const;

    [[nodiscard]] int CompletedLaps() const { return completedLaps_; }
    [[nodiscard]] bool IsWrongWay() const { return isWrongWay_; }
    [[nodiscard]] std::optional<std::size_t> LastCheckpoint() const { return last_; }

private:
    void UpdateWrongWay(Milliseconds elapsed, bool isFacingAhead);

    std::optional<std::size_t> last_;
    GateSide lastSide_ = GateSide::Forward;
    int lapIndex_ = -1;
    int completedLaps_ = 0;
    bool isPastLineBackward_ = false;
    bool isTurningWrong_ = false;
    bool isWrongWay_ = false;
    Milliseconds wrongWayTimer_{0};
};

}
