#include "sim/time_trial/time_trial_ghosts.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <utility>

namespace rr::sim {
namespace {

Milliseconds LapsTime(const std::optional<assets::GhostRun>& run, std::size_t lapCount) {
    if (!run) {
        return Milliseconds{0};
    }
    const auto laps = run->laps.begin();
    return std::accumulate(laps, laps + static_cast<std::ptrdiff_t>(lapCount), Milliseconds{0});
}

}

TimeTrialGhosts::TimeTrialGhosts(std::optional<assets::GhostRun> record) {
    if (record && !record->samples.empty()) {
        record_ = std::move(record);
    }
}

void TimeTrialGhosts::Start() {
    isRunning_ = true;
    isBestShown_ = best_.has_value();
}

void TimeTrialGhosts::Tick(Milliseconds elapsed, const Transform& playerPose) {
    if (!isRunning_) {
        return;
    }
    time_ += elapsed;
    recorder_.Record(elapsed, playerPose);
    isRecordDone_ = isRecordDone_ || HasRunOut(record_);
    isBestDone_ = isBestDone_ || (isBestShown_ && HasRunOut(best_));
}

void TimeTrialGhosts::Finish(const assets::GhostLapTimes& laps) {
    assets::GhostRun run = recorder_.Take(laps);
    const Milliseconds bestTime = best_ ? assets::RaceTime(*best_) : Milliseconds{0};
    if (bestTime == Milliseconds{0} || assets::RaceTime(run) < bestTime) {
        best_ = std::move(run);
    }
    isRunning_ = false;
    isBestShown_ = false;
}

void TimeTrialGhosts::Restart() {
    recorder_ = {};
    time_ = Milliseconds{0};
    isRunning_ = false;
    isRecordDone_ = false;
    isBestDone_ = false;
}

bool TimeTrialGhosts::HasBeatenRecord() const {
    return record_ && best_ && assets::RaceTime(*best_) < assets::RaceTime(*record_);
}

Milliseconds TimeTrialGhosts::TimeBehind(std::span<const Milliseconds> completedLaps) const {
    const std::size_t lapCount = std::min(completedLaps.size(), assets::kGhostLapCount);
    const Milliseconds best = isBestShown_ ? LapsTime(best_, lapCount) : Milliseconds{0};
    Milliseconds target = LapsTime(record_, lapCount);
    if ((best != Milliseconds{0} && target > best) || target == Milliseconds{0}) {
        target = best;
    }
    const Milliseconds player =
        std::accumulate(completedLaps.begin(), completedLaps.end(), Milliseconds{0});
    if (target != Milliseconds{0} && target < player) {
        return player - target;
    }
    return Milliseconds{0};
}

std::optional<GhostFrame> TimeTrialGhosts::RecordFrame() const {
    if (!record_ || isRecordDone_) {
        return std::nullopt;
    }
    return FrameOf(*record_);
}

std::optional<GhostFrame> TimeTrialGhosts::BestFrame() const {
    if (!best_ || !isBestShown_ || isBestDone_) {
        return std::nullopt;
    }
    return FrameOf(*best_);
}

std::optional<GhostFrame> TimeTrialGhosts::FrameOf(const assets::GhostRun& run) const {
    if (time_ >= kGhostDrawLimit) {
        return std::nullopt;
    }
    if (!isRunning_) {
        return GhostFrame{.pose = run.start};
    }
    return GhostFrameAt(run, time_);
}

bool TimeTrialGhosts::HasRunOut(const std::optional<assets::GhostRun>& run) const {
    return run && !GhostFrameAt(*run, time_);
}

}
