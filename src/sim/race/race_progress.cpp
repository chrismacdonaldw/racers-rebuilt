#include "sim/race/race_progress.hpp"

#include <numeric>

#include "core/assert.hpp"

namespace rr::sim {

RaceProgress::RaceProgress(std::size_t racerCount, int lapCount)
    : racers_(racerCount), standings_(racerCount), order_(racerCount), lapCount_(lapCount) {
    RR_ASSERT(lapCount > 0);
    for (RacerProgress& racer : racers_) {
        racer.lapTimes.assign(static_cast<std::size_t>(lapCount), Milliseconds{0});
    }
    std::ranges::iota(order_, std::size_t{0});
}

void RaceProgress::Move(const CheckpointGraph& graph, std::size_t racer, const RacerMove& move,
                        Milliseconds elapsed, bool isRunning) {
    RacerProgress& progress = racers_.at(racer);
    if (const auto crossing = graph.FindCrossing(move.from, move.to)) {
        progress.laps.Cross(graph, *crossing);
    }
    progress.laps.Update(graph, elapsed, move.facing);
    if (!isRunning || progress.finishPlace) {
        return;
    }
    const int lap = progress.laps.CompletedLaps();
    if (lap < lapCount_) {
        progress.lapTimes[static_cast<std::size_t>(lap)] += elapsed;
    } else {
        TakeNextPlace(progress);
    }
}

void RaceProgress::Rank(const CheckpointGraph& graph, std::span<const Vector3> positions) {
    RR_ASSERT(positions.size() == racers_.size());
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        const RacerProgress& racer = racers_[i];
        const std::optional<std::size_t> last = racer.laps.LastCheckpoint();
        standings_[i] = {
            .progress = racer.laps.Progress(graph),
            .distanceToNextGate = last ? graph.DistanceToNextGate(*last, positions[i])
                                       : SignedDistance(graph.Checkpoints().front(), positions[i]),
            .finishPlace = racer.finishPlace,
        };
    }
    RankRacers(standings_, order_);
}

void RaceProgress::FinishRemaining() {
    for (RacerProgress& racer : racers_) {
        if (!racer.finishPlace) {
            TakeNextPlace(racer);
        }
    }
}

void RaceProgress::TakeNextPlace(RacerProgress& racer) {
    racer.finishPlace = ++finishedCount_;
}

}
