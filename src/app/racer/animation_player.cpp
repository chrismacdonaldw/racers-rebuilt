#include "app/racer/animation_player.hpp"

#include <algorithm>
#include <utility>

#include "core/assert.hpp"

namespace rr::app {

Result<AnimationPlayer> AnimationPlayer::Create(const assets::AnimationAsset& animations,
                                                std::size_t nodeCount) {
    return sim::CheckPartTracks(animations, nodeCount).transform([&] {
        return AnimationPlayer(animations, nodeCount);
    });
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
    next_ = sim::PartClock{.part = part};
    blendMs_ = blendMs;
    blendedMs_ = 0.0F;
}

void AnimationPlayer::Advance(float elapsedMs) {
    sim::AdvanceClock(*animations_, current_, elapsedMs);
    if (next_) {
        sim::AdvanceClock(*animations_, *next_, elapsedMs);
    }
    blendedMs_ += elapsedMs;
    if (next_ && blendedMs_ >= blendMs_) {
        current_ = *std::exchange(next_, std::nullopt);
    }
}

void AnimationPlayer::AdvanceFrames(float frames) {
    sim::AdvanceFrames(*animations_, current_, frames);
}

bool AnimationPlayer::IsPlayingOnce() const {
    return !next_ && !current_.loops && !sim::IsHoldingLastFrame(*animations_, current_);
}

void AnimationPlayer::PoseNodes(std::span<Transform> locals) {
    RR_ASSERT(locals.size() == blended_.size());
    sim::SamplePart(*animations_, current_, locals);
    if (!next_) {
        return;
    }
    std::ranges::copy(locals, blended_.begin());
    sim::SamplePart(*animations_, *next_, blended_);
    const float alpha = blendMs_ > 0.0F ? std::min(blendedMs_ / blendMs_, 1.0F) : 1.0F;
    for (std::size_t i = 0; i < locals.size(); ++i) {
        locals[i] = Interpolate(locals[i], blended_[i], alpha);
    }
}

}
