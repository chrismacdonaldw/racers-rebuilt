#include "sim/race/race_progress.hpp"

#include <algorithm>
#include <numeric>

#include "core/assert.hpp"

namespace rr::sim {
namespace {

Milliseconds Sum(std::span<const Milliseconds> times) {
    return std::accumulate(times.begin(), times.end(), Milliseconds{0});
}

std::size_t CountedLaps(const RacerProgress& racer, int lapCount) {
    return static_cast<std::size_t>(std::min(racer.laps.CompletedLaps(), lapCount));
}

}

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
    const int lapsBefore = progress.laps.CompletedLaps();
    if (const auto crossing = graph.FindCrossing(move.from, move.to)) {
        progress.laps.Cross(graph, *crossing);
    }
    const int laps = progress.laps.CompletedLaps();
    if (laps > lapsBefore && laps < lapCount_) {
        progress.lapGap = GapToLeader(racer);
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

std::optional<std::size_t> RaceProgress::NextGate(const CheckpointGraph& graph,
                                                  std::size_t racer) const {
    return OfRacer(racer).laps.LastCheckpoint().transform(
        [&](std::size_t last) -> std::size_t { return graph.Checkpoints()[last].links[0]; });
}

void RaceProgress::TakeNextPlace(RacerProgress& racer) {
    racer.finishPlace = ++finishedCount_;
}

Milliseconds RaceProgress::GapToLeader(std::size_t racer) const {
    if (order_.front() == racer) {
        return Milliseconds{0};
    }
    const std::size_t laps = CountedLaps(racers_[racer], lapCount_);
    const Milliseconds ownTime = Sum(std::span(racers_[racer].lapTimes).first(laps));
    Milliseconds gap{0};
    for (std::size_t other = 0; other < racers_.size(); ++other) {
        const std::size_t otherLaps = CountedLaps(racers_[other], lapCount_);
        if (other == racer || otherLaps < laps) {
            continue;
        }
        const std::span<const Milliseconds> times = racers_[other].lapTimes;
        gap = std::max(
            gap, ownTime - Sum(times.first(laps)) + Sum(times.subspan(laps, otherLaps - laps)));
    }
    return gap;
}

}
