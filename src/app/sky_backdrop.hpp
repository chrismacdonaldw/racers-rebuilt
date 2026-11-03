#pragma once

#include <utility>
#include <vector>

#include "app/track_model.hpp"
#include "assets/geometry_asset.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"
#include "render/renderer.hpp"
#include "sim/track/race_sky.hpp"

namespace rr::app {

/// The original's 11-segment dome without caps, in engine space around the origin. Its two ranges
/// are the top band, then the band down to the horizon. Each band's vertex colour runs from black
/// at its lower ring to white at its upper ring.
[[nodiscard]] assets::GeometryAsset SkyDomeGeometry();

/// The race sky: the gradient dome and the sky world's models, kept at the camera.
class SkyBackdrop {
public:
    SkyBackdrop(render::MeshId dome, std::vector<SkyModel> models)
        : dome_(dome), models_(std::move(models)) {}

    /// `models` are the track's `SkyModel`s.
    [[nodiscard]] static Result<SkyBackdrop> Upload(render::Renderer& renderer,
                                                    std::vector<SkyModel> models);

    /// Overwrites `draws` with the sky seen from `camera`, an engine-space position. `shownMs` is
    /// the race time shown so far, which scrolls the sky world's textures.
    void Place(const sim::RaceSky& sky, const Vector3& camera, float shownMs,
               std::vector<render::MeshDraw>& draws) const;

private:
    render::MeshId dome_;
    std::vector<SkyModel> models_;
};

}
