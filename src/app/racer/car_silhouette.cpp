#include "app/racer/car_silhouette.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "app/racer/posed_draws.hpp"
#include "app/racer/racer_bounds.hpp"
#include "app/racer/racer_draws.hpp"
#include "app/racer/shadow_silhouette.hpp"
#include "core/log.hpp"
#include "core/transform.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kForwardPart = "frwrd";
constexpr float kCaptureScale = 1.15F;

void AppendCorners(const assets::GeometryAsset& model, std::span<const Transform> nodes,
                   std::vector<Vector3>& corners) {
    for (const std::uint32_t index : model.indices) {
        const assets::GeometryVertex& vertex = model.vertices.at(index);
        corners.push_back(vertex.joint < nodes.size() ? Apply(nodes[vertex.joint], vertex.position)
                                                      : vertex.position);
    }
}

Result<std::vector<Transform>> ForwardPose(const RacerAssets& racer,
                                           const assets::GeometryAsset& wheels) {
    Result<AnimatedSkeleton> skeleton =
        AnimatedSkeleton::Bind(racer.wheels, FollowJoints(wheels, {}));
    if (!skeleton) {
        return std::unexpected(std::move(skeleton.error()));
    }
    const std::optional<std::size_t> forward = skeleton->player.FindPart(kForwardPart);
    if (!forward) {
        return Fail(ErrorCode::NotFound, "the wheels have no forward animation");
    }
    skeleton->player.Play(*forward, true);
    const std::span<const Transform> nodes = skeleton->Solve();
    return std::vector<Transform>(nodes.begin(), nodes.end());
}

}

Result<std::vector<Vector3>> SilhouetteCorners(const RacerAssets& racer) {
    std::vector<Vector3> corners;
    const assets::GeometryAsset* wheels = nullptr;
    return FindRacerModel(racer, racer.files.chassis.wheels.model.geometry)
        .and_then([&](const assets::GeometryAsset* found) {
            wheels = found;
            return ForwardPose(racer, *found);
        })
        .and_then([&](const std::vector<Transform>& nodes) {
            AppendCorners(*wheels, nodes, corners);
            return FindRacerModel(racer, racer.files.champion.body.geometry);
        })
        .transform([&](const assets::GeometryAsset* body) {
            AppendCorners(*body, {}, corners);
            return std::move(corners);
        });
}

std::vector<ShadowCaster> ShadowCasters(render::Renderer& renderer, const RacerGrid& racers) {
    std::vector<ShadowCaster> casters;
    casters.reserve(racers.Count());
    for (std::size_t slot = 0; slot < racers.Count(); ++slot) {
        const RacerAssets& racer = racers.Assets(slot);
        ShadowCaster& caster = casters.emplace_back(ShadowCaster{
            .chassis = racers.Chassis(slot),
            .centre = WheelBounds(racer)
                          .transform([](const BoundingSphere& bounds) { return bounds.centre; })
                          .value_or(Vector3{})});
        Result<render::TextureId> texture =
            SilhouetteCorners(racer).and_then([&](const std::vector<Vector3>& corners) {
                return renderer.UploadTexture(
                    ShadowSilhouette(corners, caster.chassis.shadowLength * kCaptureScale,
                                     caster.chassis.shadowWidth * kCaptureScale));
            });
        if (texture) {
            caster.silhouette = *texture;
        } else {
            log::Warn("slot {} shadow: {}, using the stock shadow", slot, texture.error().message);
        }
    }
    return casters;
}

}
