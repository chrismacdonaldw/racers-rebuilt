#include "sim/track/race_sky.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace rr::sim {
namespace {

assets::Rgb8 Lerp(const assets::Rgb8& from, const assets::Rgb8& to, float amount) {
    assets::Rgb8 result{};
    for (std::size_t channel = 0; channel < result.size(); ++channel) {
        result[channel] =
            static_cast<std::uint8_t>((static_cast<float>(from[channel]) * (1.0F - amount)) +
                                      (static_cast<float>(to[channel]) * amount));
    }
    return result;
}

SkyColors Lerp(const SkyColors& from, const SkyColors& to, float amount) {
    return {.horizon = Lerp(from.horizon, to.horizon, amount),
            .middle = Lerp(from.middle, to.middle, amount),
            .top = Lerp(from.top, to.top, amount)};
}

SkyColors KeyframeColors(const assets::SkyKeyframe& keyframe) {
    return {.horizon = keyframe.horizon, .middle = keyframe.middle, .top = keyframe.top};
}

float Fraction(Milliseconds part, Milliseconds whole) {
    return whole > Milliseconds{0}
               ? static_cast<float>(part.count()) / static_cast<float>(whole.count())
               : 0.0F;
}

}

RaceSky::RaceSky(const assets::SkyFile& file)
    : file_(&file), clocks_(file.skies.size()), current_(file.initial) {
    if (!file.skies.empty()) {
        colors_ = ColorsOf(current_);
    }
}

void RaceSky::Apply(const assets::SkyStateResource& state) {
    if (!state.sky.empty()) {
        FadeTo(state.sky, state.fade);
    }
    if (state.showsDome) {
        isDomeHidden_ = false;
    }
    if (state.hidesDome) {
        isDomeHidden_ = true;
    }
    if (state.showsSkyWorld) {
        isWorldHidden_ = false;
    }
    if (state.hidesSkyWorld) {
        isWorldHidden_ = true;
    }
}

void RaceSky::FadeTo(std::string_view sky, Milliseconds fade) {
    const std::optional<std::size_t> found = Find(sky);
    if (!found) {
        return;
    }
    previous_ = std::exchange(current_, *found);
    if (current_ != previous_) {
        faded_ = Milliseconds{0};
        fade_ = std::max(fade, Milliseconds{0});
    }
}

void RaceSky::Tick(Milliseconds elapsed) {
    if (file_->skies.empty()) {
        return;
    }
    for (std::size_t sky = 0; sky < clocks_.size(); ++sky) {
        Clock& clock = clocks_[sky];
        const std::vector<assets::SkyKeyframe>& keyframes = file_->skies[sky].keyframes;
        clock.elapsed += elapsed;
        if (clock.elapsed > keyframes[clock.keyframe].length) {
            clock.keyframe = (clock.keyframe + 1) % keyframes.size();
            clock.elapsed = Milliseconds{0};
        }
    }
    if (fade_ > Milliseconds{0}) {
        faded_ += elapsed;
        if (faded_ > fade_) {
            fade_ = Milliseconds{0};
            faded_ = Milliseconds{0};
        }
    }
    colors_ = ColorsOf(current_);
    if (fade_ > Milliseconds{0}) {
        colors_ = Lerp(ColorsOf(previous_), colors_, Fraction(faded_, fade_));
    }
}

std::optional<std::size_t> RaceSky::Find(std::string_view sky) const {
    const auto found = std::ranges::find_if(file_->skies, [&](const assets::SkyGradient& gradient) {
        return assets::IsSameSkyName(gradient.name, sky);
    });
    if (found == file_->skies.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - file_->skies.begin());
}

SkyColors RaceSky::ColorsOf(std::size_t sky) const {
    const std::vector<assets::SkyKeyframe>& keyframes = file_->skies[sky].keyframes;
    const Clock& clock = clocks_[sky];
    const assets::SkyKeyframe& keyframe = keyframes[clock.keyframe];
    if (keyframes.size() == 1) {
        return KeyframeColors(keyframe);
    }
    const assets::SkyKeyframe& next = keyframes[(clock.keyframe + 1) % keyframes.size()];
    return Lerp(KeyframeColors(keyframe), KeyframeColors(next),
                Fraction(clock.elapsed, keyframe.length));
}

}
