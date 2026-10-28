#include "sim/physics/wall_sweep.hpp"

#include <algorithm>

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

}

std::optional<WallHit> SweepBodyPoints(const BodyState& body, const BodyConstants& constants,
                                       const CollisionQuery& world) {
    std::optional<WallHit> deepest;
    float deepestDistanceSquared = 0.0F;
    for (const Vector3& point : constants.bodyPoints) {
        const Vector3 from = body.saved.position + ToWorld(body.saved.basis, point);
        const Vector3 to = body.motion.position + ToWorld(body.motion.basis, point);
        const std::optional<SegmentHit> hit = world.CastSegment(from, to);
        if (!hit || !hit->isSolid) {
            continue;
        }
        const Vector3 overshoot = hit->point - to;
        const float distanceSquared = Dot(overshoot, overshoot);
        const bool isBehindPlane = Dot(overshoot, hit->normal) >= 0.0F;
        if (isBehindPlane && (!deepest || distanceSquared > deepestDistanceSquared)) {
            deepestDistanceSquared = distanceSquared;
            deepest = WallHit{.normal = hit->normal, .fraction = hit->fraction};
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
