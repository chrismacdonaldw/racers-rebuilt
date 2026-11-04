#pragma once

#include "assets/image_asset.hpp"
#include "core/vector3.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// One ground cell: a faint fill with brighter edges.
[[nodiscard]] assets::ImageAsset GroundCellImage();

/// Fills `camera`'s view just inside its far plane, shading from `colour` at the top to a darker
/// shade at the bottom.
[[nodiscard]] render::QuadDraw BackdropQuad(const render::Camera& camera,
                                            const render::Color& colour);

/// Lays `texture` once per cell over `cells` across and deep from `corner`, in game space.
[[nodiscard]] render::QuadDraw GroundQuad(render::TextureId texture, const Vector3& corner,
                                          float cellsAcross, float cellsDeep);

}
