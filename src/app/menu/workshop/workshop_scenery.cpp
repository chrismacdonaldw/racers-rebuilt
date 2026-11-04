#include "app/menu/workshop/workshop_scenery.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

#include "app/track_space.hpp"
#include "core/transform.hpp"

namespace rr::app {
namespace {

constexpr std::uint32_t kCellPx = 32;
constexpr std::uint32_t kEdgePx = 1;
constexpr std::byte kFillAlpha{24};
constexpr std::byte kEdgeAlpha{96};
constexpr std::byte kOpaque{255};
constexpr float kFarShare = 0.95F;
constexpr float kWidestAspect = 4.0F;
constexpr float kShadeBelow = 0.4F;
constexpr float kDegrees = 180.0F;

bool IsEdge(std::uint32_t px) {
    return px < kEdgePx || px >= kCellPx - kEdgePx;
}

}

assets::ImageAsset GroundCellImage() {
    assets::ImageAsset image{.widthPx = kCellPx, .heightPx = kCellPx};
    image.rgba.reserve(static_cast<std::size_t>(kCellPx) * kCellPx * 4);
    for (std::uint32_t y = 0; y < kCellPx; ++y) {
        for (std::uint32_t x = 0; x < kCellPx; ++x) {
            const std::byte alpha = IsEdge(x) || IsEdge(y) ? kEdgeAlpha : kFillAlpha;
            image.rgba.insert(image.rgba.end(), {kOpaque, kOpaque, kOpaque, alpha});
        }
    }
    return image;
}

render::QuadDraw BackdropQuad(const render::Camera& camera, const render::Color& colour) {
    const float depth = camera.farPlane * kFarShare;
    const float halfHigh =
        depth * std::tan(camera.verticalFovDegrees * std::numbers::pi_v<float> / (2.0F * kDegrees));
    const float halfWide = halfHigh * kWidestAspect;
    const auto at = [&](float x, float y) {
        return Apply(camera.transform, Vector3{.x = x, .y = y, .z = -depth});
    };
    const render::Color below{
        .r = colour.r * kShadeBelow, .g = colour.g * kShadeBelow, .b = colour.b * kShadeBelow};
    return {.corners = {at(-halfWide, halfHigh), at(halfWide, halfHigh), at(-halfWide, -halfHigh),
                        at(halfWide, -halfHigh)},
            .cornerTints = {colour, colour, below, below},
            .blend = render::BlendMode::Opaque};
}

render::QuadDraw GroundQuad(render::TextureId texture, const Vector3& corner, float cellsAcross,
                            float cellsDeep) {
    const auto at = [&](float x, float y) { return ToEngine(corner + Vector3{.x = x, .y = y}); };
    return {.texture = texture,
            .corners = {at(0.0F, cellsDeep), at(cellsAcross, cellsDeep), at(0.0F, 0.0F),
                        at(cellsAcross, 0.0F)},
            .uv = {.width = cellsAcross, .height = cellsDeep},
            .sampling = {.address = render::TextureAddress::Repeat}};
}

}
