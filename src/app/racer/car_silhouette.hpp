#pragma once

#include <optional>
#include <vector>

#include "app/racer/racer_assets.hpp"
#include "app/racer/racer_grid.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/physics/chassis.hpp"

namespace rr::app {

/// `centre` is in the car's frame. Without a `silhouette`, the stock shadow texture stays.
struct ShadowCaster {
    sim::ChassisSpec chassis;
    Vector3 centre;
    std::optional<render::TextureId> silhouette;
};

/// Three per triangle, in car space: the wheels at their first forward frame and the body.
[[nodiscard]] Result<std::vector<Vector3>> SilhouetteCorners(const RacerAssets& racer);

/// Uploads one silhouette per slot, logging any car that can't be drawn.
[[nodiscard]] std::vector<ShadowCaster> ShadowCasters(render::Renderer& renderer,
                                                      const RacerGrid& racers);

}
