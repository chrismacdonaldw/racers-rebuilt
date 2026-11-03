#include "sim/animation/part_clock.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>

namespace rr::sim {
namespace {

constexpr float kMsPerSecond = 1000.0F;

struct SampleTime {
    float frame = 0.0F;
    float spanFrames = 0.0F;
};

template <typename Value, typename Blend>
std::optional<Value> SampleRun(const assets::KeyRun& run, std::span<const Value> pool,
                               std::span<const std::uint16_t> frames, SampleTime time,
                               Blend blend) {
    if (run.count == 0) {
        return std::nullopt;
    }
    const std::span<const Value> values = pool.subspan(run.firstValue, run.count);
    if (run.count == 1) {
        return values.front();
    }
    const std::span<const std::uint16_t> times = frames.subspan(run.firstFrame, run.count);
    const auto frameOf = [](std::uint16_t key) { return static_cast<float>(key); };
    const auto next = std::ranges::upper_bound(times, time.frame, {}, frameOf);
    if (next != times.begin() && next != times.end()) {
        const auto after = static_cast<std::size_t>(next - times.begin());
        const float from = frameOf(times[after - 1]);
        return blend(values[after - 1], values[after],
                     (time.frame - from) / (frameOf(times[after]) - from));
    }
    const float last = frameOf(times.back());
    const float gap = time.spanFrames + frameOf(times.front()) - last;
    if (!(gap > 0.0F)) {
        return values.back();
    }
    const float offset =
        next == times.end() ? time.frame - last : time.frame + time.spanFrames - last;
    return blend(values.back(), values.front(), std::clamp(offset / gap, 0.0F, 1.0F));
}

float LastFrame(const assets::AnimationPart& part) {
    return static_cast<float>(std::max(part.frameCount - 1, 0));
}

}

Result<void> CheckPartTracks(const assets::AnimationAsset& animations, std::size_t nodeCount) {
    for (const assets::AnimationPart& part : animations.parts) {
        if (part.firstTrack + nodeCount > animations.tracks.size()) {
            return Fail(
                ErrorCode::InvalidData,
                std::format("part {} has tracks for fewer than {} nodes", part.name, nodeCount));
        }
    }
    return {};
}

void AdvanceFrames(const assets::AnimationAsset& animations, PartClock& clock, float frames) {
    const assets::AnimationPart& part = animations.parts[clock.part];
    clock.frame += frames;
    const float last = LastFrame(part);
    if (clock.frame >= last) {
        clock.frame =
            clock.loops ? std::fmod(clock.frame, static_cast<float>(part.loopFrames)) : last;
    }
}

void AdvanceClock(const assets::AnimationAsset& animations, PartClock& clock, float elapsedMs) {
    const auto rate = static_cast<float>(animations.parts[clock.part].framesPerSecond);
    AdvanceFrames(animations, clock, elapsedMs * rate / kMsPerSecond);
}

void RewindClock(const assets::AnimationAsset& animations, PartClock& clock, float elapsedMs) {
    const assets::AnimationPart& part = animations.parts[clock.part];
    clock.frame -= elapsedMs * static_cast<float>(part.framesPerSecond) / kMsPerSecond;
    if (clock.frame < 0.0F) {
        const auto loop = static_cast<float>(part.loopFrames);
        clock.frame = clock.loops ? std::fmod(clock.frame, loop) + loop : 0.0F;
    }
}

bool IsHoldingLastFrame(const assets::AnimationAsset& animations, const PartClock& clock) {
    return !clock.loops && clock.frame >= LastFrame(animations.parts[clock.part]);
}

float OnceLengthMs(const assets::AnimationPart& part) {
    const auto rate = static_cast<float>(part.framesPerSecond);
    return rate > 0.0F ? LastFrame(part) * kMsPerSecond / rate : 0.0F;
}

void SampleNode(const assets::AnimationAsset& animations, const PartClock& clock, std::size_t node,
                Transform& local) {
    const assets::AnimationPart& part = animations.parts[clock.part];
    const SampleTime time{
        .frame = clock.frame,
        .spanFrames = static_cast<float>(clock.loops ? part.loopFrames : part.frameCount)};
    const assets::AnimationTrack& track = animations.tracks[part.firstTrack + node];
    if (const auto rotation = SampleRun<Quaternion>(
            track.rotations, animations.rotationKeys, animations.keyFrames, time,
            [](const Quaternion& from, const Quaternion& to, float alpha) {
                return Nlerp(from, to, alpha);
            })) {
        local.rotation = *rotation;
    }
    if (const auto position =
            SampleRun<Vector3>(track.positions, animations.positionKeys, animations.keyFrames, time,
                               [](const Vector3& from, const Vector3& to, float alpha) {
                                   return Lerp(from, to, alpha);
                               })) {
        local.position = *position;
    }
}

void SamplePart(const assets::AnimationAsset& animations, const PartClock& clock,
                std::span<Transform> locals) {
    for (std::size_t node = 0; node < locals.size(); ++node) {
        SampleNode(animations, clock, node, locals[node]);
    }
}

PartClock InterpolateClock(const PartClock& previous, const PartClock& next, float alpha) {
    if (previous.part != next.part || previous.loops != next.loops || next.frame < previous.frame) {
        return next;
    }
    PartClock blended = next;
    blended.frame = previous.frame + ((next.frame - previous.frame) * alpha);
    return blended;
}

}
