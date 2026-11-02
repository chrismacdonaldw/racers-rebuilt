#include "sim/replay/replay_recorder.hpp"

#include <utility>

#include "core/assert.hpp"
#include "sim/replay/snapshot_hash.hpp"

namespace rr::sim {

ReplayRecorder::ReplayRecorder(std::string buildVersion, std::string scene, std::uint64_t seed,
                               std::vector<std::string> gameplay)
    : replay_{.buildVersion = std::move(buildVersion),
              .scene = std::move(scene),
              .seed = seed,
              .gameplay = std::move(gameplay)} {}

void ReplayRecorder::Record(const InputFrame& input, const RenderSnapshot& after) {
    replay_.frames.push_back(input);
    hashes_.push_back(HashSnapshot(after));
}

ReplayPlayer::ReplayPlayer(Replay replay, std::vector<std::uint64_t> hashes)
    : replay_(std::move(replay)), hashes_(std::move(hashes)) {}

bool ReplayPlayer::IsFinished() const {
    return next_ >= replay_.frames.size();
}

const InputFrame& ReplayPlayer::Next() {
    RR_VERIFY(!IsFinished());
    return replay_.frames[next_++];
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
