#include "app/ground_decal.hpp"

#include <optional>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kGameUp{.z = 1.0F};
constexpr Vector3 kGameForward{.x = 1.0F};
constexpr float kCastAbove = 6.0F;
constexpr float kCastDepth = 15.0F;
constexpr float kMinWallUp = 0.1F;

Vector3 AlongGround(const Vector3& level, const Vector3& normal) {
    return {
        .x = level.x, .y = level.y, .z = -((normal.x * level.x) + (normal.y * level.y)) / normal.z};
}

}

void AppendGroundDecal(const QuadLook& look, const sim::CollisionQuery& ground,
                       const GroundDecal& decal, std::vector<render::QuadDraw>& out) {
    const Vector3 from = decal.centre + (kGameUp * kCastAbove);
    const std::optional<sim::SegmentHit> hit =
        sim::CastSolid(ground, from, from - (kGameUp * kCastDepth));
    if (!hit || hit->normal.z < kMinWallUp) {
        return;
    }
    const Vector3 along = Normalized({.x = decal.forward.x, .y = decal.forward.y}, kGameForward);
    const Vector3 top = AlongGround(along * (decal.length * 0.5F), hit->normal);
    const Vector3 right = AlongGround(Cross(along, kGameUp) * (decal.width * 0.5F), hit->normal);
    AppendGroundQuad(look, ToEngine(hit->point), ToEngine(hit->normal), ToEngine(top),
                     ToEngine(right), out);
}

}
