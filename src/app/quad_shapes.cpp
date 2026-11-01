#include "app/quad_shapes.hpp"

#include <cstddef>

#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kCameraUp{.y = 1.0F};
constexpr Vector3 kCameraRight{.x = 1.0F};
constexpr Vector3 kEngineUp{.y = 1.0F};
constexpr Vector3 kEngineAcross{.x = 1.0F};
constexpr Vector3 kEngineAlong{.z = 1.0F};
constexpr float kDecalLift = 0.1F;

}

QuadLook LookOf(const render::Material& material) {
    return {.texture = material.texture,
            .tint = material.tint,
            .blend = material.blend == render::BlendMode::Opaque ? render::BlendMode::Alpha
                                                                 : material.blend};
}

render::Color Modulate(const render::Color& a, const render::Color& b) {
    return {.r = a.r * b.r, .g = a.g * b.g, .b = a.b * b.b, .a = a.a * b.a};
}

void AppendBillboard(const QuadLook& look, const Vector3& centre, float width, float height,
                     const Transform& camera, std::vector<render::QuadDraw>& out) {
    const Vector3 top = Rotate(camera.rotation, kCameraUp) * (height * 0.5F);
    const Vector3 side = Rotate(camera.rotation, kCameraRight) * (width * 0.5F);
    out.push_back({.texture = look.texture,
                   .corners = {centre + top - side, centre + top + side, centre - top - side,
                               centre - top + side},
                   .tint = look.tint,
                   .blend = look.blend});
}

void AppendDecal(const QuadLook& look, const Vector3& centre, float size,
                 std::vector<render::QuadDraw>& out) {
    const Vector3 lifted = centre + (kEngineUp * kDecalLift);
    const Vector3 across = kEngineAcross * (size * 0.5F);
    const Vector3 along = kEngineAlong * (size * 0.5F);
    out.push_back({.texture = look.texture,
                   .corners = {lifted - along - across, lifted - along + across,
                               lifted + along - across, lifted + along + across},
                   .tint = look.tint,
                   .blend = look.blend});
}

void AppendRibbon(const QuadLook& look, std::span<const Vector3> points, float width,
                  const Transform& camera, std::vector<render::QuadDraw>& out) {
    for (std::size_t i = 1; i < points.size(); ++i) {
        const Vector3& from = points[i - 1];
        const Vector3& to = points[i];
        const Vector3 toCamera = camera.position - ((from + to) * 0.5F);
        const Vector3 fallback = Rotate(camera.rotation, kCameraUp);
        const Vector3 side = Normalized(Cross(to - from, toCamera), fallback) * (width * 0.5F);
        out.push_back({.texture = look.texture,
                       .corners = {from - side, to - side, from + side, to + side},
                       .tint = look.tint,
                       .blend = look.blend});
    }
}

}
