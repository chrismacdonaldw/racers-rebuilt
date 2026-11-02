#include "app/time_trial/ghost_car.hpp"

#include <format>

namespace rr::app {
namespace {

constexpr float kGhostAlpha = 127.0F / 255.0F;
constexpr std::size_t kChassisPart = 0;
constexpr float kMsPerSecond = 1000.0F;

ModelDraws Ghostly(ModelDraws model) {
    for (render::MeshDraw& draw : model.draws) {
        render::Material& material = draw.material;
        material.tint.a *= kGhostAlpha;
        material.alphaCutoff *= kGhostAlpha;
        if (material.blend == render::BlendMode::Opaque) {
            material.blend = render::BlendMode::Alpha;
        }
    }
    return model;
}

}

Result<GhostCar> GhostCar::Create(GhostCarModels models) {
    if (models.chassisJoints == nullptr || models.driverJoints == nullptr) {
        return Fail(ErrorCode::InvalidData, "a ghost's chassis and driver need skeletons");
    }
    const std::size_t driverParts = models.driverJoints->animations.parts.size();
    if (models.driverPart >= driverParts) {
        return Fail(ErrorCode::InvalidData, std::format("a ghost's driver has no part {} of {}",
                                                        models.driverPart, driverParts));
    }
    Result<AnimatedSkeleton> chassis =
        AnimatedSkeleton::Bind(*models.chassisJoints, models.chassis);
    Result<AnimatedSkeleton> driver = chassis.and_then([&](const AnimatedSkeleton&) {
        return AnimatedSkeleton::Bind(*models.driverJoints, models.driver);
    });
    if (!driver) {
        return std::unexpected(std::move(driver.error()));
    }
    Parts parts{.chassis = std::move(*chassis), .driver = std::move(*driver), .seat = models.seat};
    parts.chassisDraws = parts.posed.Append(Ghostly(std::move(models.chassis)));
    parts.bodyDraws = parts.posed.Append(Ghostly(std::move(models.body)));
    parts.driverDraws = parts.posed.Append(Ghostly(std::move(models.driver)));
    parts.chassis.player.Play(kChassisPart, true);
    parts.driver.player.Play(models.driverPart, true);
    return GhostCar(std::move(parts));
}

void GhostCar::Update(const std::optional<sim::GhostFrame>& frame, float elapsedMs) {
    isShown_ = frame.has_value();
    if (!frame) {
        return;
    }
    parts_.chassis.player.AdvanceFrames(frame->chassisAnimationRate * elapsedMs / kMsPerSecond);
    parts_.driver.player.Advance(elapsedMs);
    const Transform& pose = frame->pose;
    const Transform seat{.position = Apply(pose, parts_.seat), .rotation = pose.rotation};
    parts_.posed.Place(parts_.chassisDraws, pose, parts_.chassis.Solve());
    parts_.posed.Place(parts_.bodyDraws, pose);
    parts_.posed.Place(parts_.driverDraws, seat, parts_.driver.Solve());
}

std::span<const render::MeshDraw> GhostCar::Draws() const {
    return isShown_ ? parts_.posed.Draws() : std::span<const render::MeshDraw>();
}

}
