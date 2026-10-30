#include "app/hud/place_pulse.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kSwellTime{175};
constexpr float kSwell = 1.0F;

}

void PlacePulse::Advance(int place, Milliseconds elapsed) {
    if (!place_) {
        place_ = place;
    } else if (place != place_) {
        place_ = place;
        sinceChange_ = Milliseconds{0};
    } else if (sinceChange_) {
        *sinceChange_ += elapsed;
        if (*sinceChange_ >= 2 * kSwellTime) {
            sinceChange_.reset();
        }
    }
}

float PlacePulse::Scale() const {
    if (!sinceChange_) {
        return 1.0F;
    }
    const Milliseconds fromPeak =
        *sinceChange_ < kSwellTime ? kSwellTime - *sinceChange_ : *sinceChange_ - kSwellTime;
    return 1.0F + (kSwell * (1.0F - (static_cast<float>(fromPeak.count()) /
                                     static_cast<float>(kSwellTime.count()))));
}

}
