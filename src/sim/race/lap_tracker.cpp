#include "sim/race/lap_tracker.hpp"

#include <algorithm>

namespace rr::sim {
namespace {

constexpr float kAheadLimit = 0.2F;
constexpr Milliseconds kWrongWayDelay{500};

}

LapTracker::LapTracker(LapCounting counting) : counting_(counting), zones_(counting.keepsOneZone) {}

void LapTracker::Cross(const CheckpointGraph& graph, const GateCrossing& crossing) {
    if (last_ == crossing.checkpoint && lastSide_ == crossing.side) {
        return;
    }
    if (graph.LapFraction(crossing.checkpoint) == 0.0F) {
        if (crossing.side == GateSide::Backward) {
            isPastLineBackward_ = true;
            zones_.Reverse();
            last_ = crossing.checkpoint;
            lastSide_ = crossing.side;
            return;
        }
        if (!isPastLineBackward_) {
            ++lapIndex_;
            if (!counting_.usesZones) {
                completedLaps_ = std::max(completedLaps_, lapIndex_);
            }
        }
        isPastLineBackward_ = false;
    } else if (isPastLineBackward_) {
        --lapIndex_;
        isPastLineBackward_ = false;
    }
    last_ = crossing.checkpoint;
    lastSide_ = crossing.side;
}

void LapTracker::ReachFinish(int lapCount) {
    completedLaps_ = std::max(completedLaps_, lapCount);
}

bool LapTracker::EnterZone(assets::LapZone zone) {
    if (!counting_.usesZones || !zones_.Enter(zone)) {
        return false;
    }
    ++finishCrossings_;
    if (finishCrossings_ > 0 && finishCrossings_ > completedLaps_) {
        ++completedLaps_;
        return true;
    }
    return false;
}

void LapTracker::StartZones() {
    zones_ = LapZones(counting_.keepsOneZone);
}

void LapTracker::Update(const CheckpointGraph& graph, Milliseconds elapsed, const Vector3& facing) {
    if (last_) {
        const assets::Checkpoint& gate = graph.Checkpoints()[*last_];
        UpdateWrongWay(elapsed, Dot(facing, gate.normal) <= kAheadLimit);
    }
}

void LapTracker::UpdateWrongWay(Milliseconds elapsed, bool isFacingAhead) {
    if (wrongWayTimer_ > Milliseconds{0}) {
        if (elapsed < wrongWayTimer_) {
            wrongWayTimer_ -= elapsed;
        } else {
            wrongWayTimer_ = Milliseconds{0};
            isWrongWay_ = false;
        }
    }
    if (isTurningWrong_) {
        if (isFacingAhead) {
            wrongWayTimer_ = Milliseconds{0};
            isTurningWrong_ = false;
        } else if (wrongWayTimer_ == Milliseconds{0}) {
            isTurningWrong_ = false;
            isWrongWay_ = true;
        }
    } else if (!isFacingAhead) {
        wrongWayTimer_ = kWrongWayDelay;
        isTurningWrong_ = !isWrongWay_;
    }
}

float LapTracker::Progress(const CheckpointGraph& graph) const {
    if (!last_) {
        return 0.0F;
    }
    return static_cast<float>(lapIndex_) + graph.LapFraction(*last_);
}

}
