#include "app/ground_decal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

#include "app/decal_clip.hpp"
#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kGameUp{.z = 1.0F};
constexpr Vector3 kGameForward{.x = 1.0F};
constexpr float kCastDepth = 15.0F;
constexpr float kTextureSpan = 0.99F;
constexpr float kCentre = 0.5F;
constexpr float kLift = 0.1F;

constexpr unsigned kBeforeU = 1U;
constexpr unsigned kAfterU = 2U;
constexpr unsigned kBeforeV = 4U;
constexpr unsigned kAfterV = 8U;

struct Projector {
    Vector3 origin;
    Vector3 down;
    Vector3 uAxis;
    Vector3 vAxis;
    float uScale = 0.0F;
    float vScale = 0.0F;

    [[nodiscard]] DecalVertex Project(const Vector3& point) const {
        const Vector3 offset = point - origin;
        return {.position = point,
                .u = (Dot(offset, uAxis) * uScale) + kCentre,
                .v = (Dot(offset, vAxis) * vScale) + kCentre};
    }

    [[nodiscard]] bool Reaches(const DecalTriangle& triangle) const {
        const auto [nearest, farthest] =
            std::minmax({Dot(triangle[0] - origin, down), Dot(triangle[1] - origin, down),
                         Dot(triangle[2] - origin, down)});
        return farthest >= 0.0F && nearest <= kCastDepth;
    }
};

Projector ProjectorOf(const GroundDecal& decal) {
    const Vector3 down = ToEngine(-kGameUp);
    const Vector3 forward = ToEngine(decal.forward);
    const Vector3 along = Normalized(forward - (down * Dot(forward, down)), ToEngine(kGameForward));
    return {.origin = ToEngine(decal.centre + (kGameUp * kDecalCastAbove)),
            .down = down,
            .uAxis = Cross(along, down),
            .vAxis = along,
            .uScale = kTextureSpan / decal.width,
            .vScale = kTextureSpan / decal.length};
}

GroundArea AreaOf(const Projector& projector) {
    const Vector3 across = projector.uAxis * (kCentre / projector.uScale);
    const Vector3 along = projector.vAxis * (kCentre / projector.vScale);
    const Vector3 deep = projector.down * kCastDepth;
    GroundArea area{.minX = projector.origin.x,
                    .minZ = projector.origin.z,
                    .maxX = projector.origin.x,
                    .maxZ = projector.origin.z};
    for (const Vector3& offset :
         {across + along, across - along, along - across, -across - along}) {
        for (const Vector3& corner :
             {projector.origin + offset, projector.origin + offset + deep}) {
            area = {.minX = std::min(area.minX, corner.x),
                    .minZ = std::min(area.minZ, corner.z),
                    .maxX = std::max(area.maxX, corner.x),
                    .maxZ = std::max(area.maxZ, corner.z)};
        }
    }
    return area;
}

unsigned OutcodeOf(const DecalVertex& vertex) {
    return (vertex.u < 0.0F ? kBeforeU : 0U) | (vertex.u > 1.0F ? kAfterU : 0U) |
           (vertex.v < 0.0F ? kBeforeV : 0U) | (vertex.v > 1.0F ? kAfterV : 0U);
}

bool FacesDecal(const std::array<DecalVertex, 3>& corners) {
    const DecalVertex& last = corners[2];
    return ((corners[0].v - last.v) * (corners[1].u - last.u)) -
               ((corners[0].u - last.u) * (corners[1].v - last.v)) >
           0.0F;
}

Vector3 LiftOf(const DecalTriangle& triangle, const Vector3& down) {
    const Vector3 normal =
        Normalized(Cross(triangle[1] - triangle[0], triangle[2] - triangle[0]), -down);
    return (Dot(normal, down) > 0.0F ? -normal : normal) * kLift;
}

render::TexturePoint UvOf(const DecalVertex& vertex) {
    return {.u = vertex.u, .v = vertex.v};
}

void AppendFan(const QuadLook& look, const DecalPolygon& polygon, const Vector3& lift,
               std::vector<render::QuadDraw>& out) {
    const DecalVertex& hub = polygon.corners[0];
    for (std::size_t corner = 1; corner + 1 < polygon.count; corner += 2) {
        const DecalVertex& first = polygon.corners.at(corner);
        const DecalVertex& second = polygon.corners.at(corner + 1);
        const DecalVertex& third =
            corner + 2 < polygon.count ? polygon.corners.at(corner + 2) : hub;
        out.push_back({.texture = look.texture,
                       .corners = {first.position + lift, second.position + lift,
                                   hub.position + lift, third.position + lift},
                       .tint = look.tint,
                       .blend = look.blend,
                       .sampling = look.sampling,
                       .cornerUvs = std::array{UvOf(first), UvOf(second), UvOf(hub), UvOf(third)}});
    }
}

void AppendTriangle(const QuadLook& look, const Projector& projector, const DecalTriangle& triangle,
                    std::vector<render::QuadDraw>& out) {
    const std::array<DecalVertex, 3> corners{projector.Project(triangle[0]),
                                             projector.Project(triangle[1]),
                                             projector.Project(triangle[2])};
    const unsigned sharedOutside =
        OutcodeOf(corners[0]) & OutcodeOf(corners[1]) & OutcodeOf(corners[2]);
    if (sharedOutside != 0U || !FacesDecal(corners) || !projector.Reaches(triangle)) {
        return;
    }
    AppendFan(look, ClipToDecal(corners), LiftOf(triangle, projector.down), out);
}

}

void GroundDecals::Append(const QuadLook& look, const GroundDecal& decal,
                          std::vector<render::QuadDraw>& out) {
    if (decal.width <= 0.0F || decal.length <= 0.0F) {
        return;
    }
    const Projector projector = ProjectorOf(decal);
    surface_->Gather(AreaOf(projector), nearby_);
    for (const std::uint32_t index : nearby_) {
        AppendTriangle(look, projector, surface_->Triangles()[index], out);
    }
}

}
