#include "app/menu/preview_cycle.hpp"

#include <algorithm>

#include "assets/race_list.hpp"

namespace rr::app {

void PreviewCycle::Restart(PreviewStyle style) {
    style_ = style;
    race_ = 0;
    timer_ = Milliseconds{0};
}

std::optional<std::size_t> PreviewCycle::Advance(Milliseconds elapsed) {
    fadeAge_ += elapsed;
    if (style_.cycles && elapsed > timer_) {
        race_ = (race_ + 1) % assets::kCircuitRaces;
        return race_;
    }
    timer_ = std::max(timer_ - elapsed, Milliseconds{0});
    if (style_.cycles && timer_ < kPreviewFadeTime && fade_ != Fade::ToBlack &&
        fadeAge_ >= kPreviewFadeTime) {
        StartFade(Fade::ToBlack);
    }
    return std::nullopt;
}

void PreviewCycle::Shown(bool restartsTimer) {
    if (restartsTimer) {
        timer_ = kPreviewRaceTime;
    }
    StartFade(style_.fades ? Fade::FromBlack : Fade::None);
}

float PreviewCycle::FadeAlpha() const {
    const float progress = std::min(
        static_cast<float>(fadeAge_.count()) / static_cast<float>(kPreviewFadeTime.count()), 1.0F);
    switch (fade_) {
        case Fade::ToBlack:
            return progress;
        case Fade::FromBlack:
            return 1.0F - progress;
        case Fade::None:
            return 0.0F;
    }
    return 0.0F;
}

void PreviewCycle::StartFade(Fade fade) {
    fade_ = fade;
    fadeAge_ = Milliseconds{0};
}

}
