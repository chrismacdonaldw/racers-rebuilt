#include "sim/physics/ground_contact.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>

#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr float kFollowRatePerMs = 0.04F;
constexpr float kContactBand = 0.4F;
constexpr float kRideHeight = 0.2F;
constexpr float kLiftOffDrop = 0.008F;
constexpr int kLandingAirborneMs = 400;
constexpr float kBounceSpeed = -0.05F;
constexpr float kBounceScale = 1.15F;
constexpr std::array<std::size_t, kWheelCount> kLengthwisePartner{2, 3, 0, 1};
constexpr std::array<std::size_t, kWheelCount> kSidewaysPartner{1, 0, 3, 2};

void CastWheelRay(WheelProbe& wheel, const CollisionQuery& query, std::size_t world,
                  float sweepHeight, float sweepDistance) {
    const Vector3 end = wheel.position - Vector3{.z = sweepDistance};
    const std::optional<SegmentHit> hit =
        query.CastSegment(world, wheel.position + Vector3{.z = sweepHeight}, end);
    if (!hit) {
        return;
    }
    wheel.hasHit = true;
    wheel.hitSurface = hit->surface;
    wheel.hitPoint = hit->point;
    wheel.hitNormal = hit->normal;
    wheel.hitHeight = hit->point.z - end.z;
}

std::optional<std::size_t> CastWheelRays(BodyState& body, const BodyConstants& constants, int dtMs,
                                         const CollisionQuery& query) {
    BodyMotion& motion = body.motion;
    const float drop = std::max(0.0F, body.saved.position.z - motion.position.z);
    const float sweepHeight = constants.boundsRadius + drop;
    const float sweepDistance =
        IsGrounded(motion) && !body.isSpunOut ? static_cast<float>(dtMs) * kFollowRatePerMs : 0.0F;
    for (WheelProbe& wheel : motion.wheels) {
        wheel.hasHit = false;
        wheel.isInContact = false;
        wheel.contactForce = {};
        wheel.hitSurface = {};
    }
    for (std::size_t world = 0; world < query.WorldCount(); ++world) {
        for (WheelProbe& wheel : motion.wheels) {
            if (!wheel.hasHit) {
                CastWheelRay(wheel, query, world, sweepHeight, sweepDistance);
            }
        }
    }
    std::optional<std::size_t> highest;
    for (std::size_t i = 0; i < kWheelCount; ++i) {
        const WheelProbe& wheel = motion.wheels[i];
        if (wheel.hasHit && (!highest || wheel.hitHeight > motion.wheels[*highest].hitHeight)) {
            highest = i;
        }
    }
    return highest;
}

void MarkContacts(BodyMotion& motion, float highestHit) {
    const float limit = std::max(0.0F, highestHit - kContactBand);
    motion.contactCount = 0;
    motion.supportNormal = {};
    for (WheelProbe& wheel : motion.wheels) {
        wheel.isInContact = wheel.hasHit && wheel.hitHeight >= limit;
        if (wheel.isInContact) {
            ++motion.contactCount;
            motion.supportNormal += wheel.hitNormal;
        }
    }
}

Vector3 PairAxis(const BodyMotion& motion, std::size_t wheel, std::size_t partner,
                 const Vector3& fallback) {
    if (!motion.wheels[partner].hasHit) {
        return fallback;
    }
    const std::size_t low = std::min(wheel, partner);
    const std::size_t high = std::max(wheel, partner);
    return motion.wheels[high].hitPoint - motion.wheels[low].hitPoint;
}

void SnapToContacts(BodyState& body, const BodyConstants& constants, std::size_t selected) {
    BodyMotion& motion = body.motion;
    const bool isTumbling = body.impulses.rollMs > 0 || body.impulses.pitchMs > 0;
    if (motion.contactCount >= 2 && !isTumbling) {
        const Vector3 backward =
            PairAxis(motion, selected, kLengthwisePartner[selected], -motion.basis.forward);
        const Vector3 side =
            PairAxis(motion, selected, kSidewaysPartner[selected], motion.basis.side);
        const Vector3 forward = -backward;
        motion.basis = OrthonormalBasis(forward, Cross(forward, side));
        PlaceWheels(motion, constants);
    }
    const WheelProbe& wheel = motion.wheels[selected];
    motion.position += wheel.hitPoint - wheel.position + Vector3{.z = kRideHeight};
    PlaceWheels(motion, constants);
}

bool BouncesOnLanding(BodyState& body, bool wasAirborne) {
    if (!wasAirborne || body.airborneMs <= kLandingAirborneMs) {
        return false;
    }
    const float into = Dot(body.motion.velocity, body.motion.supportNormal);
    if (into >= 0.0F) {
        return false;
    }
    if (into < kBounceSpeed) {
        body.motion.velocity.z -= kBounceScale * into;
        ClearContacts(body.motion);
        return true;
    }
    body.motion.velocity.z -= into;
    return false;
}

void ApplySupportForces(BodyState& body, const BodyConstants& constants) {
    BodyMotion& motion = body.motion;
    const float gravityForce = -kGravity * constants.massScale;
    const Vector3& normal = motion.supportNormal;
    body.supportForce = (Vector3{.z = 1.0F} - (normal * normal.z)) * gravityForce;
    const Vector3 push{.z = -gravityForce / static_cast<float>(motion.contactCount + 8)};
    for (WheelProbe& wheel : motion.wheels) {
        wheel.contactForce = wheel.isInContact ? push : Vector3{};
    }
}

}

void PlaceWheels(BodyMotion& motion, const BodyConstants& constants) {
    const Basis& basis = motion.basis;
    std::array<WheelProbe, kWheelCount>& wheels = motion.wheels;
    wheels[1].position = motion.position + ToWorld(basis, constants.wheelAnchor);
    wheels[0].position = wheels[1].position - (basis.side * constants.trackWidth);
    wheels[3].position = wheels[1].position - (basis.forward * constants.wheelbase);
    wheels[2].position = wheels[0].position - (basis.forward * constants.wheelbase);
}

void UpdateGroundContact(BodyState& body, const BodyConstants& constants, int dtMs,
                         const CollisionQuery& world) {
    BodyMotion& motion = body.motion;
    if (body.derived.isAtRest && motion.velocity == Vector3{} &&
        body.angularVelocity == Vector3{}) {
        return;
    }
    body.derived.isAtRest = false;
    const bool wasAirborne = !IsGrounded(motion);
    PlaceWheels(motion, constants);
    const std::optional<std::size_t> highest = CastWheelRays(body, constants, dtMs, world);
    if (!highest) {
        if (!wasAirborne) {
            motion.velocity.z -= kLiftOffDrop;
        }
        ClearContacts(motion);
        return;
    }
    MarkContacts(motion, motion.wheels[*highest].hitHeight);
    const std::size_t selected = motion.contactCount == kWheelCount ? 0 : *highest;
    SnapToContacts(body, constants, selected);
    if (motion.contactCount >= 3) {
        for (WheelProbe& wheel : motion.wheels) {
            wheel.isInContact = true;
        }
        motion.contactCount = kWheelCount;
        motion.supportNormal = motion.basis.up;
    } else {
        motion.supportNormal = Normalized(motion.supportNormal, motion.basis.up);
    }
    if (BouncesOnLanding(body, wasAirborne)) {
        return;
    }
    ApplySupportForces(body, constants);
}

void ClearContacts(BodyMotion& motion) {
    motion.contactCount = 0;
    for (WheelProbe& wheel : motion.wheels) {
        wheel.isInContact = false;
    }
}

void UpdateWheelSurfaces(BodyMotion& motion) {
    for (WheelProbe& wheel : motion.wheels) {
        wheel.surface = wheel.isInContact ? wheel.hitSurface : Surface{};
    }
}

float AverageContactGrip(const BodyMotion& motion) {
    float total = 0.0F;
    int contacts = 0;
    for (const WheelProbe& wheel : motion.wheels) {
        if (wheel.isInContact) {
            total += wheel.surface.lateralGrip;
            ++contacts;
        }
    }
    return contacts > 0 ? total / static_cast<float>(contacts) : 0.0F;
}

}
