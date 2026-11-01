#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "app/powerup/powerup_models.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/transform.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/animation/skeleton_pose.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// The `TURBO3.WDB` tunnel the original draws around a warping player instead of the track.
// NOLINTNEXTLINE(bugprone-exception-escape)
class WarpTube {
public:
    /// Logs a failure and returns nothing, so a warp then flies over the track.
    [[nodiscard]] static std::optional<WarpTube> Load(render::Renderer& renderer,
                                                      const assets::Vfs& vfs,
                                                      assets::TrackLoader& loader);

    WarpTube(std::unique_ptr<PowerupAssets> assets, EffectModels models);

    /// Moves `car`, drawn at the engine-space `carPose`, to the tube's origin facing game +X.
    void Fly(float leftMs, const Transform& carPose, std::span<const render::MeshDraw> car);

    [[nodiscard]] std::span<const render::MeshDraw> Draws() const { return draws_; }

private:
    struct Part {
        const EffectModel* model = nullptr;
        sim::SkeletonPose pose;
        assets::TextureScroll scroll;
    };

    void Place(Part& part, float flownMs);

    std::unique_ptr<PowerupAssets> assets_;
    EffectModels models_;
    std::vector<Part> parts_;
    std::vector<render::MeshDraw> draws_;
};

[[nodiscard]] const sim::EffectSnapshot* FindWarpFlight(
    std::span<const sim::EffectSnapshot> effects, std::size_t racer);

/// Chases the car in the tube, the field of view swelling up to 45 degrees mid-flight.
[[nodiscard]] render::Camera WarpFlightCamera(render::Camera base, float leftMs);

}
