#include "app/powerup/warp_tube.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>
#include <utility>

#include "app/chase_camera.hpp"
#include "app/track_space.hpp"
#include "core/log.hpp"
#include "sim/animation/part_clock.hpp"
#include "sim/powerup/warp_effect.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kTubeWorld = "TURBO3.WDB";
constexpr float kFlightMs = 1500.0F;
constexpr float kFovSwellDegrees = 45.0F;
constexpr float kMsPerSecond = 1000.0F;

float Wrapped(float offset) {
    return offset - std::floor(offset);
}

}

std::optional<WarpTube> WarpTube::Load(render::Renderer& renderer, const assets::Vfs& vfs,
                                       assets::TrackLoader& loader) {
    Result<std::unique_ptr<PowerupAssets>> assets =
        LoadEffectAssets(vfs, loader, kTubeWorld).transform([](PowerupAssets loaded) {
            return std::make_unique<PowerupAssets>(std::move(loaded));
        });
    Result<EffectModels> models = assets.and_then([&](const std::unique_ptr<PowerupAssets>& owned) {
        return UploadPowerupModels(renderer, *owned);
    });
    if (!models) {
        log::Warn("warp tube: {}", models.error().message);
        return std::nullopt;
    }
    return WarpTube(std::move(*assets), std::move(*models));
}

WarpTube::WarpTube(std::unique_ptr<PowerupAssets> assets, EffectModels models)
    : assets_(std::move(assets)), models_(std::move(models)) {
    for (const auto& [name, model] : models_) {
        if (model.jointed != nullptr) {
            parts_.push_back({.model = &model,
                              .pose = sim::SkeletonPose(model.jointed->skeleton, model.draws.scale),
                              .scroll = model.scroll});
        }
    }
}

void WarpTube::Fly(float leftMs, const Transform& carPose, std::span<const render::MeshDraw> car) {
    draws_.clear();
    for (Part& part : parts_) {
        Place(part, kFlightMs - leftMs);
    }
    const Transform toTube = Compose(TransformToEngine({}), Inverse(carPose));
    for (render::MeshDraw draw : car) {
        draw.transform = Compose(toTube, draw.transform);
        draws_.push_back(draw);
    }
}

void WarpTube::Place(Part& part, float flownMs) {
    const JointedAssets& jointed = *part.model->jointed;
    sim::PartClock clock;
    sim::AdvanceClock(jointed.animations, clock, flownMs);
    sim::SamplePart(jointed.animations, clock, part.pose.Locals());
    const std::span<const Transform> nodes = part.pose.Solve();
    const float seconds = flownMs / kMsPerSecond;
    const render::UvOffset scrolled{.u = Wrapped(part.scroll.uPerSecond * seconds),
                                    .v = Wrapped(part.scroll.vPerSecond * seconds)};
    const ModelDraws& model = part.model->draws;
    for (std::size_t i = 0; i < model.draws.size(); ++i) {
        render::MeshDraw draw = model.draws[i];
        const std::uint16_t joint = i < model.joints.size() ? model.joints[i] : 0;
        const Transform node = joint < nodes.size() ? nodes[joint] : Transform{};
        draw.transform = TransformToEngine(Compose(part.model->placement, node));
        draw.material.uvOffset = scrolled;
        draws_.push_back(draw);
    }
}

const sim::EffectSnapshot* FindWarpFlight(std::span<const sim::EffectSnapshot> effects,
                                          std::size_t racer) {
    const auto flight = std::ranges::find_if(effects, [&](const sim::EffectSnapshot& effect) {
        return effect.kind == sim::PowerupKind::Warp && effect.use.racer == racer &&
               effect.phase == static_cast<std::uint8_t>(sim::WarpPhase::Active);
    });
    return flight == effects.end() ? nullptr : &*flight;
}

render::Camera WarpFlightCamera(render::Camera base, float leftMs) {
    const Transform car = CarToEngine({});
    base.transform = ChaseCamera(car).Pose(car);
    base.verticalFovDegrees +=
        kFovSwellDegrees * std::sin(std::numbers::pi_v<float> * leftMs / kFlightMs);
    return base;
}

}
