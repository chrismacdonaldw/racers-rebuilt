#include "app/racer/racer_visuals.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string_view>
#include <utility>

#include "app/track_space.hpp"

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

Result<sim::SkeletonPose> MakePose(const JointedAssets& jointed, const ModelDraws& model) {
    const std::size_t nodeCount = jointed.skeleton.nodes.size();
    if (std::ranges::any_of(model.joints,
                            [&](std::uint16_t joint) { return joint >= nodeCount; })) {
        return Fail(ErrorCode::InvalidData,
                    std::format("a model follows a node past the skeleton's {}", nodeCount));
    }
    return sim::SkeletonPose(jointed.skeleton, model.scale);
}

Result<void> Bind(const JointedAssets& jointed, const ModelDraws& model,
                  std::optional<sim::SkeletonPose>& pose, std::optional<AnimationPlayer>& player) {
    Result<sim::SkeletonPose> made = MakePose(jointed, model);
    Result<AnimationPlayer> created = made.and_then([&](const sim::SkeletonPose&) {
        return AnimationPlayer::Create(jointed.animations, jointed.skeleton.nodes.size());
    });
    if (!created) {
        return std::unexpected(std::move(created.error()));
    }
    pose.emplace(std::move(*made));
    player.emplace(std::move(*created));
    return {};
}

}

Result<RacerVisuals> RacerVisuals::Create(const RacerAssets& assets, RacerDraws draws) {
    std::optional<sim::SkeletonPose> wheelPose;
    std::optional<sim::SkeletonPose> driverPose;
    std::optional<AnimationPlayer> wheelPlayer;
    std::optional<AnimationPlayer> driverPlayer;
    Result<WheelParts> wheelParts =
        Bind(assets.wheels, draws.wheels, wheelPose, wheelPlayer)
            .and_then([&] { return Bind(assets.driver, draws.driver, driverPose, driverPlayer); })
            .and_then([&] { return FindWheelParts(*wheelPlayer); });
    Result<DriverParts> driverParts =
        wheelParts.and_then([&](const WheelParts&) { return FindDriverParts(*driverPlayer); });
    if (!driverParts) {
        return std::unexpected(std::move(driverParts.error()));
    }
    Parts parts{.wheels = {.pose = std::move(*wheelPose), .player = std::move(*wheelPlayer)},
                .driver = {.pose = std::move(*driverPose), .player = std::move(*driverPlayer)},
                .wheelParts = *wheelParts,
                .driverParts = *driverParts,
                .seat = assets.files.chassis.seat,
                .mass = assets.files.champion.mass};
    parts.wheelDraws = parts.Append(draws.wheels);
    parts.bodyDraws = parts.Append(draws.body);
    parts.driverDraws = parts.Append(draws.driver);
    parts.wheels.player.Play(parts.wheelParts.forward, true);
    parts.driver.player.Play(parts.driverParts.straight, true);
    return RacerVisuals(std::move(parts));
}

RacerVisuals::Group RacerVisuals::Parts::Append(const ModelDraws& model) {
    const Group group{.first = draws.size(), .count = model.draws.size()};
    draws.insert(draws.end(), model.draws.begin(), model.draws.end());
    joints.insert(joints.end(), model.joints.begin(), model.joints.end());
    joints.resize(draws.size());
    return group;
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
    Place(parts_.wheelDraws, wheels, parts_.wheels.pose.Solve());
    Place(parts_.bodyDraws, body, {});
    Place(parts_.driverDraws, seat, parts_.driver.pose.Solve());
}

void RacerVisuals::Spin(float forwardSpeed, float elapsedMs) {
    AnimationPlayer& player = parts_.wheels.player;
    player.Play(forwardSpeed < 0.0F ? parts_.wheelParts.reverse : parts_.wheelParts.forward, true);
    const float speed = std::abs(forwardSpeed);
    player.AdvanceFrames(speed < kStillSpeed ? 0.0F : speed * kWheelFramesPerSpeed * elapsedMs);
    player.PoseNodes(parts_.wheels.pose.Locals());
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
    player.PoseNodes(parts_.driver.pose.Locals());
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

void RacerVisuals::Place(Group group, const Transform& entity, std::span<const Transform> nodes) {
    for (std::size_t i = group.first; i < group.first + group.count; ++i) {
        const std::uint16_t joint = parts_.joints[i];
        const Transform placed = joint < nodes.size() ? Compose(entity, nodes[joint]) : entity;
        parts_.draws[i].transform = TransformToEngine(placed);
    }
}

}
