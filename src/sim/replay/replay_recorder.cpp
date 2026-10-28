#include "sim/replay/replay_recorder.hpp"

#include <utility>

#include "core/assert.hpp"
#include "sim/replay/snapshot_hash.hpp"

namespace rr::sim {

ReplayRecorder::ReplayRecorder(std::string buildVersion, std::string scene, std::uint64_t seed)
    : replay_{.buildVersion = std::move(buildVersion),
              .scene = std::move(scene),
              .seed = seed,
              .racers = {{}}} {}

void ReplayRecorder::Record(const InputFrame& input, const RenderSnapshot& after) {
    replay_.racers.front().push_back(input);
    hashes_.push_back(HashSnapshot(after));
}

ReplayPlayer::ReplayPlayer(Replay replay, std::vector<std::uint64_t> hashes)
    : replay_(std::move(replay)), hashes_(std::move(hashes)) {}

bool ReplayPlayer::IsFinished() const {
    return replay_.racers.empty() || next_ >= replay_.racers.front().size();
}

const InputFrame& ReplayPlayer::Next() {
    RR_VERIFY(!IsFinished());
    return replay_.racers.front()[next_++];
}

void ReplayPlayer::Check(const RenderSnapshot& after) {
    const std::size_t tick = next_ - 1;
    if (tick >= hashes_.size()) {
        return;
    }
    ++checked_;
    if (!firstMismatch_ && HashSnapshot(after) != hashes_[tick]) {
        firstMismatch_ = tick + 1;
    }
}

}
