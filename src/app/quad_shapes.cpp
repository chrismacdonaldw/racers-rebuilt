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

void AppendGroundQuad(const QuadLook& look, const Vector3& centre, const Vector3& normal,
                      const Vector3& top, const Vector3& right,
                      std::vector<render::QuadDraw>& out) {
    const Vector3 lifted = centre + (normal * kDecalLift);
    out.push_back({.texture = look.texture,
                   .corners = {lifted + top - right, lifted + top + right, lifted - top - right,
                               lifted - top + right},
                   .tint = look.tint,
                   .blend = look.blend});
}

void AppendDecal(const QuadLook& look, const Vector3& centre, float size,
                 std::vector<render::QuadDraw>& out) {
    const float half = size * 0.5F;
    AppendGroundQuad(look, centre, kEngineUp, kEngineAlong * -half, kEngineAcross * half, out);
}

void AppendRibbon(const QuadLook& look, std::span<const RibbonPoint> points, float width,
                  const Transform& camera, std::vector<render::QuadDraw>& out) {
    for (std::size_t i = 1; i < points.size(); ++i) {
        const RibbonPoint& from = points[i - 1];
        const RibbonPoint& to = points[i];
        const Vector3 toCamera = camera.position - ((from.position + to.position) * 0.5F);
        const Vector3 fallback = Rotate(camera.rotation, kCameraUp);
        const Vector3 side =
            Normalized(Cross(to.position - from.position, toCamera), fallback) * (width * 0.5F);
        out.push_back({.texture = look.texture,
                       .corners = {from.position - side, to.position - side, from.position + side,
                                   to.position + side},
                       .tint = look.tint,
                       .cornerTints = {from.tint, to.tint, from.tint, to.tint},
                       .blend = look.blend});
    }
}

}
