#include "sim/time_trial/ghost_playback.hpp"

#include <cstddef>

namespace rr::sim {
namespace {

constexpr float kChassisRateScale = 4.0F * 1.8F;

}

std::optional<GhostFrame> GhostFrameAt(const assets::GhostRun& run, Milliseconds time) {
    const auto index = static_cast<std::size_t>(time / kGhostSampleInterval);
    if (index + 1 >= run.samples.size()) {
        return std::nullopt;
    }
    const float alpha = static_cast<float>((time % kGhostSampleInterval).count()) /
                        static_cast<float>(kGhostSampleInterval.count());
    const Transform from = assets::FromGhostSample(run.samples[index]);
    const Transform to = assets::FromGhostSample(run.samples[index + 1]);
    return GhostFrame{
        .pose = Interpolate(from, to, alpha),
        .chassisAnimationRate = Length(to.position - from.position) * kChassisRateScale};
}

}
