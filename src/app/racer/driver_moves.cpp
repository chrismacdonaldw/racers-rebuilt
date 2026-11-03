#include "app/racer/driver_moves.hpp"

#include <algorithm>
#include <initializer_list>

namespace rr::app {
namespace {

constexpr float kSideLookLimit = 0.2F;
constexpr float kRearLookLimit = -0.6F;
constexpr std::uint32_t kFirstLookMs = 2000;
constexpr std::uint32_t kLookCooldownMs = 5000;
constexpr std::uint32_t kLookCooldownSpreadMs = 1000;

[[nodiscard]] bool IsAny(DriverPart part, std::initializer_list<DriverPart> parts) {
    return std::ranges::find(parts, part) != parts.end();
}

[[nodiscard]] DriverCue Once(DriverPart part, DriverPart then = DriverPart::Default) {
    return {.part = part, .kind = DriverCueKind::Once, .then = then};
}

[[nodiscard]] DriverCue Loop(DriverPart part) {
    return {.part = part, .kind = DriverCueKind::Loop};
}

}

DriverMoves::DriverMoves(sim::Random random)
    : random_(random), lookCooldownMs_(static_cast<float>(random_.Below(kFirstLookMs))) {}

std::optional<DriverCue> DriverMoves::Next(const DriverSense& sense, bool isPlayingOnce,
                                           float elapsedMs) {
    lookCooldownMs_ = std::max(lookCooldownMs_ - elapsedMs, 0.0F);
    std::optional<DriverCue> settle;
    if (queued_ && !isPlayingOnce) {
        settle = Loop(*queued_);
        Take(*settle);
    }
    std::optional<DriverCue> cue = Decide(sense);
    if (cue) {
        Take(*cue);
        return cue;
    }
    return settle;
}

std::optional<DriverCue> DriverMoves::Decide(const DriverSense& sense) {
    if (sense.finishPlace) {
        return Finish(*sense.finishPlace);
    }
    if (sense.hasStopped && current_ != DriverPart::SuddenStop) {
        return Once(DriverPart::SuddenStop);
    }
    if (sense.wasHit) {
        return Once(DriverPart::Hit);
    }
    if (IsHeld()) {
        return std::nullopt;
    }
    if (sense.isReversing) {
        if (IsAny(current_, {DriverPart::Reverse, DriverPart::ReverseHold})) {
            return std::nullopt;
        }
        return Once(DriverPart::Reverse, DriverPart::ReverseHold);
    }
    if (current_ == DriverPart::ReverseHold) {
        return Once(DriverPart::ReverseBack);
    }
    if (IsAny(current_, {DriverPart::SideLookRight, DriverPart::SideLookLeft,
                         DriverPart::RearLookRight, DriverPart::RearLookLeft})) {
        return std::nullopt;
    }
    if (lookCooldownMs_ <= 0.0F && sense.nearestRacer) {
        if (std::optional<DriverCue> look = Look(*sense.nearestRacer)) {
            return look;
        }
    }
    return Steer(sense.turnRadius);
}

std::optional<DriverCue> DriverMoves::Finish(int place) {
    if (place != 1) {
        if (IsAny(current_, {DriverPart::Loser, DriverPart::LoserLoop})) {
            return std::nullopt;
        }
        return Once(DriverPart::Loser, DriverPart::LoserLoop);
    }
    if (IsAny(current_, {DriverPart::Winner, DriverPart::WinnerStart, DriverPart::WinnerLoop})) {
        return std::nullopt;
    }
    if ((random_.Below(sim::kRollRange) & 1U) == 0) {
        return Loop(DriverPart::Winner);
    }
    return Once(DriverPart::WinnerStart, DriverPart::WinnerLoop);
}

std::optional<DriverCue> DriverMoves::Look(const Vector3& nearest) {
    std::optional<DriverPart> look;
    const bool isLeft = nearest.y >= 0.0F;
    if (nearest.x > -kSideLookLimit && nearest.x < kSideLookLimit) {
        look = isLeft ? DriverPart::SideLookLeft : DriverPart::SideLookRight;
    } else if (nearest.x > kRearLookLimit && nearest.x <= -kSideLookLimit) {
        look = isLeft ? DriverPart::RearLookLeft : DriverPart::RearLookRight;
    }
    if (!look) {
        return std::nullopt;
    }
    lookCooldownMs_ = static_cast<float>(kLookCooldownMs +
                                         (random_.Below(sim::kRollRange) % kLookCooldownSpreadMs));
    return Once(*look);
}

std::optional<DriverCue> DriverMoves::Steer(float turnRadius) const {
    DriverPart part = DriverPart::Default;
    if (turnRadius < 0.0F) {
        part = DriverPart::SteerRight;
    } else if (turnRadius > 0.0F) {
        part = DriverPart::SteerLeft;
    }
    if (part == current_) {
        return std::nullopt;
    }
    return DriverCue{.part = part, .kind = DriverCueKind::Blend};
}

bool DriverMoves::IsHeld() const {
    return IsAny(current_, {DriverPart::SuddenStop, DriverPart::Hit, DriverPart::Winner,
                            DriverPart::WinnerLoop, DriverPart::LoserLoop});
}

void DriverMoves::Take(const DriverCue& cue) {
    current_ = cue.part;
    queued_.reset();
    if (cue.kind == DriverCueKind::Once) {
        queued_ = cue.then;
    }
}

}
