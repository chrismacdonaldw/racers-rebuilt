#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "core/duration.hpp"
#include "core/vector3.hpp"
#include "sim/race/checkpoint_graph.hpp"
#include "sim/race/lap_tracker.hpp"
#include "sim/race/standings.hpp"

namespace rr::sim {

struct RacerMove {
    Vector3 from;
    Vector3 to;
    Vector3 facing;
};

/// `lapGap` is how far the racer trailed the leader when it last started a lap.
struct RacerProgress {
    LapTracker laps;
    std::vector<Milliseconds> lapTimes;
    std::optional<int> finishPlace;
    Milliseconds lapGap{0};
};

class RaceProgress {
public:
    RaceProgress(std::size_t racerCount, int lapCount);

    /// While `isRunning`, times the current lap and gives a racer reaching the lap count a place.
    void Move(const CheckpointGraph& graph, std::size_t racer, const RacerMove& move,
              Milliseconds elapsed, bool isRunning);

    void Rank(const CheckpointGraph& graph, std::span<const Vector3> positions);

    /// Gives every unfinished racer the remaining places, in racer order.
    void FinishRemaining();

    [[nodiscard]] const RacerProgress& OfRacer(std::size_t racer) const {
        return racers_.at(racer);
    }

    /// Racer indexes, first place first, as of the last `Rank`.
    [[nodiscard]] std::span<const std::size_t> Standings() const { return order_; }

    [[nodiscard]] int LapCount() const { return lapCount_; }

private:
    void TakeNextPlace(RacerProgress& racer);
    [[nodiscard]] Milliseconds GapToLeader(std::size_t racer) const;

    std::vector<RacerProgress> racers_;
    std::vector<RacerStanding> standings_;
    std::vector<std::size_t> order_;
    int lapCount_ = 0;
    int finishedCount_ = 0;
};

}
