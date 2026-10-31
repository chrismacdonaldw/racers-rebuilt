#include "app/racer/animation_player.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

#include "core/assert.hpp"

namespace rr::app {
namespace {

constexpr float kMsPerSecond = 1000.0F;

constexpr auto kBlendRotations = [](const Quaternion& from, const Quaternion& to, float alpha) {
    return Nlerp(from, to, alpha);
};

constexpr auto kBlendPositions = [](const Vector3& from, const Vector3& to, float alpha) {
    return Lerp(from, to, alpha);
};

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

}

Result<AnimationPlayer> AnimationPlayer::Create(const assets::AnimationAsset& animations,
                                                std::size_t nodeCount) {
    for (const assets::AnimationPart& part : animations.parts) {
        if (part.firstTrack + nodeCount > animations.tracks.size()) {
            return Fail(
                ErrorCode::InvalidData,
                std::format("part {} has tracks for fewer than {} nodes", part.name, nodeCount));
        }
    }
    return AnimationPlayer(animations, nodeCount);
}

std::optional<std::size_t> AnimationPlayer::FindPart(std::string_view name) const {
    const auto part = std::ranges::find(animations_->parts, name, &assets::AnimationPart::name);
    if (part == animations_->parts.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(part - animations_->parts.begin());
}

void AnimationPlayer::Play(std::size_t part, bool loops) {
    if (!next_ && current_.part == part && current_.loops == loops) {
        return;
    }
    current_ = {.part = part, .loops = loops};
    next_.reset();
}

void AnimationPlayer::BlendTo(std::size_t part, float blendMs) {
    if ((next_ ? next_->part : current_.part) == part) {
        return;
    }
    if (next_) {
        current_ = *next_;
    }
    next_ = Clock{.part = part};
    blendMs_ = blendMs;
    blendedMs_ = 0.0F;
}

void AnimationPlayer::Advance(float elapsedMs) {
    const auto run = [&](Clock& clock) {
        const auto rate = static_cast<float>(animations_->parts[clock.part].framesPerSecond);
        clock.frame += elapsedMs * rate / kMsPerSecond;
        Wrap(clock);
    };
    run(current_);
    if (next_) {
        run(*next_);
    }
    blendedMs_ += elapsedMs;
    if (next_ && blendedMs_ >= blendMs_) {
        current_ = *std::exchange(next_, std::nullopt);
    }
}

void AnimationPlayer::AdvanceFrames(float frames) {
    current_.frame += frames;
    Wrap(current_);
}

bool AnimationPlayer::IsPlayingOnce() const {
    const assets::AnimationPart& part = animations_->parts[current_.part];
    return !next_ && !current_.loops &&
           current_.frame < static_cast<float>(std::max(part.frameCount - 1, 0));
}

void AnimationPlayer::PoseNodes(std::span<Transform> locals) {
    RR_ASSERT(locals.size() == blended_.size());
    Sample(current_, locals);
    if (!next_) {
        return;
    }
    std::ranges::copy(locals, blended_.begin());
    Sample(*next_, blended_);
    const float alpha = blendMs_ > 0.0F ? std::min(blendedMs_ / blendMs_, 1.0F) : 1.0F;
    for (std::size_t i = 0; i < locals.size(); ++i) {
        locals[i] = Interpolate(locals[i], blended_[i], alpha);
    }
}

void AnimationPlayer::Wrap(Clock& clock) const {
    const assets::AnimationPart& part = animations_->parts[clock.part];
    const auto last = static_cast<float>(std::max(part.frameCount - 1, 0));
    if (clock.frame >= last) {
        clock.frame =
            clock.loops ? std::fmod(clock.frame, static_cast<float>(part.loopFrames)) : last;
    }
}

void AnimationPlayer::Sample(const Clock& clock, std::span<Transform> locals) const {
    const assets::AnimationAsset& animations = *animations_;
    const assets::AnimationPart& part = animations.parts[clock.part];
    const SampleTime time{
        .frame = clock.frame,
        .spanFrames = static_cast<float>(clock.loops ? part.loopFrames : part.frameCount)};
    for (std::size_t node = 0; node < locals.size(); ++node) {
        const assets::AnimationTrack& track = animations.tracks[part.firstTrack + node];
        if (const auto rotation =
                SampleRun<Quaternion>(track.rotations, animations.rotationKeys,
                                      animations.keyFrames, time, kBlendRotations)) {
            locals[node].rotation = *rotation;
        }
        if (const auto position = SampleRun<Vector3>(track.positions, animations.positionKeys,
                                                     animations.keyFrames, time, kBlendPositions)) {
            locals[node].position = *position;
        }
    }
}

}
