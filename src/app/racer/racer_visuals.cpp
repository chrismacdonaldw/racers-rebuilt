#include "app/racer/racer_visuals.hpp"

#include <cmath>
#include <format>
#include <optional>
#include <string_view>
#include <utility>

namespace rr::app {
namespace {

constexpr float kWheelFramesPerSpeed = 1.8F;
constexpr float kStillSpeed = 0.0001F;
constexpr float kSteerBlendMs = 300.0F;

Result<std::size_t> FindPart(const AnimationPlayer& player, std::string_view name) {
    const std::optional<std::size_t> part = player.FindPart(name);
    if (!part) {
        return Fail(ErrorCode::NotFound, std::format("no animation part named {}", name));
    }
    return *part;
}

}

Result<RacerVisuals> RacerVisuals::Create(const RacerAssets& assets, RacerDraws draws) {
    Result<AnimatedSkeleton> wheels = AnimatedSkeleton::Bind(assets.wheels, draws.wheels);
    Result<AnimatedSkeleton> driver = wheels.and_then([&](const AnimatedSkeleton&) {
        return AnimatedSkeleton::Bind(assets.driver, draws.driver);
    });
    Result<WheelParts> wheelParts =
        driver.and_then([&](const AnimatedSkeleton&) { return FindWheelParts(wheels->player); });
    Result<DriverParts> driverParts =
        wheelParts.and_then([&](const WheelParts&) { return FindDriverParts(driver->player); });
    if (!driverParts) {
        return std::unexpected(std::move(driverParts.error()));
    }
    Parts parts{.wheels = std::move(*wheels),
                .driver = std::move(*driver),
                .wheelParts = *wheelParts,
                .driverParts = *driverParts,
                .seat = assets.files.chassis.seat,
                .mass = assets.files.champion.mass};
    parts.wheelDraws = parts.posed.Append(draws.wheels);
    parts.bodyDraws = parts.posed.Append(draws.body);
    parts.driverDraws = parts.posed.Append(draws.driver);
    parts.wheels.player.Play(parts.wheelParts.forward, true);
    parts.driver.player.Play(parts.driverParts.straight, true);
    return RacerVisuals(std::move(parts));
}

Result<RacerVisuals::WheelParts> RacerVisuals::FindWheelParts(const AnimationPlayer& player) {
    WheelParts parts;
    return FindPart(player, "frwrd")
        .and_then([&](std::size_t forward) {
            parts.forward = forward;
            return FindPart(player, "rvrse");
        })
        .transform([&](std::size_t reverse) {
            parts.reverse = reverse;
            return parts;
        });
}

Result<RacerVisuals::DriverParts> RacerVisuals::FindDriverParts(const AnimationPlayer& player) {
    DriverParts parts;
    return FindPart(player, "default")
        .and_then([&](std::size_t straight) {
            parts.straight = straight;
            return FindPart(player, "steer-r");
        })
        .and_then([&](std::size_t steerRight) {
            parts.steerRight = steerRight;
            return FindPart(player, "steer-l");
        })
        .and_then([&](std::size_t steerLeft) {
            parts.steerLeft = steerLeft;
            return FindPart(player, "power1");
        })
        .transform([&](std::size_t hit) {
            parts.hit = hit;
            return parts;
        });
}

void RacerVisuals::Update(const Transform& wheels, const sim::RacerSnapshot& motion,
                          float elapsedMs) {
    Spin(motion.forwardSpeed, elapsedMs);
    Drive(motion, elapsedMs);
    const Transform body = lean_.Update(wheels, elapsedMs, parts_.mass);
    const Transform seat{.position = Apply(body, parts_.seat), .rotation = body.rotation};
    parts_.posed.Place(parts_.wheelDraws, wheels, parts_.wheels.Solve());
    parts_.posed.Place(parts_.bodyDraws, body);
    parts_.posed.Place(parts_.driverDraws, seat, parts_.driver.Solve());
}

void RacerVisuals::Spin(float forwardSpeed, float elapsedMs) {
    AnimationPlayer& player = parts_.wheels.player;
    player.Play(forwardSpeed < 0.0F ? parts_.wheelParts.reverse : parts_.wheelParts.forward, true);
    const float speed = std::abs(forwardSpeed);
    player.AdvanceFrames(speed < kStillSpeed ? 0.0F : speed * kWheelFramesPerSpeed * elapsedMs);
}

void RacerVisuals::Drive(const sim::RacerSnapshot& motion, float elapsedMs) {
    AnimationPlayer& player = parts_.driver.player;
    if (std::exchange(hitReactions_, motion.hitReactions) != motion.hitReactions) {
        player.Play(parts_.driverParts.hit, false);
        isReacting_ = true;
    } else if (!player.IsPlayingOnce()) {
        if (std::exchange(isReacting_, false)) {
            player.Play(parts_.driverParts.straight, true);
        }
        Steer(motion.turnRadius);
    }
    player.Advance(elapsedMs);
}

void RacerVisuals::Steer(float turnRadius) {
    AnimationPlayer& player = parts_.driver.player;
    const DriverParts& driverParts = parts_.driverParts;
    std::size_t part = driverParts.straight;
    if (turnRadius < 0.0F) {
        part = driverParts.steerRight;
    } else if (turnRadius > 0.0F) {
        part = driverParts.steerLeft;
    }
    player.BlendTo(part, kSteerBlendMs);
}

}
