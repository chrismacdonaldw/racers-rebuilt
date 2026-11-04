#include "app/menu/tile_drawing.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "app/menu/stand_scene.hpp"
#include "assets/world_database.hpp"

namespace rr::app {
namespace {

constexpr float kTileFov = 30.0F;
constexpr float kTileFill = 1.15F;
constexpr Vector3 kTileLook{.x = 0.6F, .y = -0.8F, .z = 0.55F};
constexpr float kOutline = 2.0F;

}

render::Camera PieceTileCamera(const BoundingSphere& bounds) {
    const float reach = std::max(bounds.radius, 0.1F) * kTileFill /
                        std::sin(kTileFov * std::numbers::pi_v<float> / 360.0F);
    const float length = std::sqrt((kTileLook.x * kTileLook.x) + (kTileLook.y * kTileLook.y) +
                                   (kTileLook.z * kTileLook.z));
    assets::WorldCamera camera;
    camera.position = bounds.centre + (kTileLook * (reach / length));
    camera.target = bounds.centre;
    camera.fovDegrees = kTileFov;
    camera.nearClip = reach / 10.0F;
    camera.farClip = reach * 10.0F;
    return MenuSceneCamera(camera);
}

void DrawOutline(const render::Rect& area, const render::Color& colour,
                 std::vector<render::SpriteDraw>& out) {
    const float right = area.x + area.width;
    const float bottom = area.y + area.height;
    for (const render::Rect& edge :
         {render::Rect{.x = area.x - kOutline,
                       .y = area.y - kOutline,
                       .width = area.width + (2.0F * kOutline),
                       .height = kOutline},
          render::Rect{.x = area.x - kOutline,
                       .y = bottom,
                       .width = area.width + (2.0F * kOutline),
                       .height = kOutline},
          render::Rect{
              .x = area.x - kOutline, .y = area.y, .width = kOutline, .height = area.height},
          render::Rect{.x = right, .y = area.y, .width = kOutline, .height = area.height}}) {
        out.push_back({.bounds = edge, .tint = colour});
    }
}

}
