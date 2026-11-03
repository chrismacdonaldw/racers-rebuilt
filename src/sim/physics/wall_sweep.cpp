#include "sim/physics/wall_sweep.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr int kWallYawMs = 200;
constexpr float kWallYawRate = 0.004F;
constexpr float kBounce = 0.3F;
constexpr float kVerticalBounce = 0.15F;
constexpr float kPushOffSpeed = 0.004F;
constexpr float kMaxRiseSpeed = 0.3F;

float WallYawRate(float sideFacing) {
    if (sideFacing < 0.0F) {
        return -kWallYawRate * (((sideFacing + 1.0F) / 2.0F) + 0.5F);
    }
    return kWallYawRate * (((1.0F - sideFacing) / 2.0F) + 0.5F);
}

void Touch(const SegmentHit& hit, SurfaceTouches& touches) {
    if (hit.surface.isFinish) {
        touches.ReachFinish();
    }
    if (hit.surface.touchEvent) {
        touches.Add(*hit.surface.touchEvent, hit.point);
    }
}

bool IsBehindPlane(const SegmentHit& hit, const Vector3& overshoot) {
    const float depth = Dot(overshoot, hit.normal);
    return hit.isTrackWorld ? depth >= 0.0F : depth > 0.0F;
}

}

std::optional<WallHit> SweepBodyPoints(const BodyState& body, const BodyConstants& constants,
                                       const CollisionQuery& query, SurfaceTouches* touches) {
    std::array<bool, kBodyPointCount> isPointHit{};
    std::optional<WallHit> deepest;
    float deepestDistanceSquared = 0.0F;
    for (std::size_t world = 0; world < query.WorldCount(); ++world) {
        for (std::size_t i = 0; i < kBodyPointCount; ++i) {
            if (isPointHit.at(i)) {
                continue;
            }
            const Vector3& point = constants.bodyPoints.at(i);
            const Vector3 from = body.saved.position + ToWorld(body.saved.basis, point);
            const Vector3 to = body.motion.position + ToWorld(body.motion.basis, point);
            const std::optional<SegmentHit> hit = query.CastSegment(world, from, to);
            if (hit && touches != nullptr) {
                Touch(*hit, *touches);
            }
            if (!hit || !hit->isSolid || (hit->isTrackWorld && !body.hasTrackCollision)) {
                continue;
            }
            const Vector3 overshoot = hit->point - to;
            const float distanceSquared = Dot(overshoot, overshoot);
            if (IsBehindPlane(*hit, overshoot) &&
                (!deepest || distanceSquared > deepestDistanceSquared)) {
                isPointHit.at(i) = true;
                deepestDistanceSquared = distanceSquared;
                deepest = WallHit{.normal = hit->normal, .fraction = hit->fraction};
            }
        }
    }
    return deepest;
}

void ApplyWallResponse(BodyState& body, const BodyConstants& constants, const Vector3& normal) {
    StopSteering(body.steering);
    Vector3& velocity = body.motion.velocity;
    const float into = Dot(velocity, normal);
    if (into < 0.0F) {
        velocity -= normal * into;
    }
    const Basis& basis = body.motion.basis;
    if (Dot(basis.forward, normal) < 0.0F) {
        SetYawImpulse(body, constants, WallYawRate(Dot(basis.side, normal)), kWallYawMs);
        body.hasWallYaw = true;
    }
    if (into < 0.0F) {
        velocity.x -= kBounce * into * normal.x;
        velocity.y -= kBounce * into * normal.y;
        velocity.z -= kVerticalBounce * into * normal.z;
    }
    velocity += normal * kPushOffSpeed;
    velocity.z = std::min(velocity.z, kMaxRiseSpeed);
}

}
