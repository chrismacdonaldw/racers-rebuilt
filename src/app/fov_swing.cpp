#include "app/fov_swing.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace rr::app {
namespace {

constexpr Milliseconds kStep{500};
constexpr float kNarrowest = 40.0F;
constexpr float kWidest = 80.0F;

[[nodiscard]] float HalfRollWithin(std::uint32_t roll, float range) {
    const auto whole = static_cast<std::uint32_t>(range);
    return whole == 0 ? 0.0F : static_cast<float>(roll % whole) * 0.5F;
}

}

FovSwing::FovSwing(float baseDegrees, sim::Random random)
    : random_(random), base_(baseDegrees), degrees_(baseDegrees) {}

void FovSwing::Tick(Milliseconds elapsed, bool isCursed, bool isGhost) {
    left_ = std::max(left_ - elapsed, Milliseconds{0});
    if (isGhost) {
        left_ = Milliseconds{0};
    } else if (left_ == Milliseconds{0}) {
        left_ = kStep;
        start_ = degrees_;
        if (isCursed) {
            amplitude_ = SwingAmplitude();
        } else if (degrees_ != base_) {
            amplitude_ = (start_ - base_) * 0.5F;
        } else {
            amplitude_ = 0.0F;
            left_ = Milliseconds{0};
        }
    }
    if (left_ == Milliseconds{0}) {
        degrees_ = base_;
        return;
    }
    const float eased = std::numbers::pi_v<float> * static_cast<float>((kStep - left_).count()) /
                        static_cast<float>(kStep.count());
    degrees_ = start_ - amplitude_ + (std::cos(eased) * amplitude_);
}

float FovSwing::SwingAmplitude() {
    const bool narrows = (amplitude_ <= 0.0F && start_ > kNarrowest) || start_ >= kWidest;
    const std::uint32_t roll = random_.Below(sim::kRollRange);
    return narrows ? HalfRollWithin(roll, start_ - kNarrowest)
                   : -HalfRollWithin(roll, kWidest - start_);
}

}
