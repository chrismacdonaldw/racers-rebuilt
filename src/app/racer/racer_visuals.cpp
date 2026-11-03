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

std::optional<Vector3> Facing(const Transform& wheels, const std::optional<Vector3>& other) {
    if (!other) {
        return std::nullopt;
    }
    const Vector3 toward = *other - wheels.position;
    if (Length(toward) <= 0.0F) {
        return std::nullopt;
    }
    return Rotate(Conjugate(wheels.rotation), Normalized(toward, {}));
}

}

Result<RacerVisuals> RacerVisuals::Create(const RacerAssets& assets, RacerDraws draws,
                                          sim::Random random) {
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
    parts.driver.player.Play(parts.driverParts[std::to_underlying(DriverPart::Default)], true);
    return RacerVisuals(std::move(parts), random);
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
    DriverParts parts{};
    for (std::size_t i = 0; i < kDriverPartCount; ++i) {
        Result<std::size_t> part = FindPart(player, kDriverPartNames[i]);
        if (!part) {
            return std::unexpected(std::move(part.error()));
        }
        parts[i] = *part;
    }
    return parts;
}

void RacerVisuals::Update(const Transform& wheels, const sim::RacerSnapshot& motion,
                          std::optional<Vector3> nearestRacer, float elapsedMs) {
    Spin(motion.forwardSpeed, elapsedMs);
    Drive({.turnRadius = motion.turnRadius,
           .isReversing = motion.isReversing,
           .wasHit = std::exchange(hitReactions_, motion.hitReactions) != motion.hitReactions,
           .hasStopped = std::exchange(suddenStops_, motion.suddenStops) != motion.suddenStops,
           .finishPlace = motion.finishPlace,
           .nearestRacer = Facing(wheels, nearestRacer)},
          elapsedMs);
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

void RacerVisuals::Drive(const DriverSense& sense, float elapsedMs) {
    AnimationPlayer& player = parts_.driver.player;
    if (const std::optional<DriverCue> cue =
            moves_.Next(sense, player.IsPlayingOnce(), elapsedMs)) {
        const std::size_t part = parts_.driverParts.at(std::to_underlying(cue->part));
        switch (cue->kind) {
            case DriverCueKind::Once:
                player.Play(part, false);
                break;
            case DriverCueKind::Loop:
                player.Play(part, true);
                break;
            case DriverCueKind::Blend:
                player.BlendTo(part, kSteerBlendMs);
                break;
        }
    }
    player.Advance(elapsedMs);
}

}
