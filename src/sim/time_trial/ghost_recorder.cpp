#include "sim/time_trial/ghost_recorder.hpp"

#include <utility>

#include "sim/time_trial/ghost_playback.hpp"

namespace rr::sim {

void GhostRecorder::Record(Milliseconds elapsed, const Transform& pose) {
    if (elapsed < untilSample_) {
        untilSample_ -= elapsed;
        return;
    }
    untilSample_ += kGhostSampleInterval - elapsed;
    if (run_.samples.size() >= assets::kGhostSampleCapacity) {
        return;
    }
    if (run_.samples.empty()) {
        run_.start = pose;
    }
    run_.samples.push_back(assets::ToGhostSample(pose));
}

assets::GhostRun GhostRecorder::Take(const assets::GhostLapTimes& laps) {
    run_.laps = laps;
    assets::GhostRun taken = std::exchange(run_, {});
    untilSample_ = Milliseconds{0};
    return taken;
}

}
