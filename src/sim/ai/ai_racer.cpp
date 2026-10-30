#include "sim/ai/ai_racer.hpp"

#include <algorithm>
#include <utility>

namespace rr::sim {
namespace {

constexpr float kSpeedUpPerMs = 1.0F / 2048.0F;
constexpr float kSlowDownPerMs = 0.002F;

}

AiRacer::AiRacer(assets::RaceRoute route) : route_(std::move(route)) {}

void AiRacer::Tick(Milliseconds elapsed) {
    if (elapsed <= Milliseconds{0}) {
        return;
    }
    const auto elapsedMs = static_cast<float>(elapsed.count());
    EaseTowardTarget(elapsedMs);
    const Vector3 from = route_.Pose().position;
    route_.Advance(elapsed, playbackSpeed_);
    velocity_ = (route_.Pose().position - from) * (1.0F / elapsedMs);
}

Vector3 AiRacer::Facing() const {
    return Rotate(route_.Pose().rotation, {.x = 1.0F});
}

void AiRacer::EaseTowardTarget(float elapsedMs) {
    if (playbackSpeed_ < targetSpeed_) {
        playbackSpeed_ = std::min(playbackSpeed_ + (kSpeedUpPerMs * elapsedMs), targetSpeed_);
    } else {
        playbackSpeed_ = std::max(playbackSpeed_ - (kSlowDownPerMs * elapsedMs), targetSpeed_);
    }
}

}
