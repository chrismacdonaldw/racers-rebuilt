#include "sim/physics/hover_contact.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

#include "sim/physics/ground_contact.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr float kMsPerSecond = 1000.0F;
constexpr float kBankRatePerMs = 0.005F;
constexpr float kMaxBank = 0.7853982F;
constexpr float kBankReferenceRadius = 4096.0F;
constexpr float kFollowRatePerMs = 0.01F;
constexpr float kContactBand = 0.4F;
constexpr float kLiftOffDrop = 0.008F;
constexpr float kBounceSpeed = -0.05F;
constexpr float kBounceScale = 1.3F;

struct AxleRay {
    Vector3 start;
    Vector3 end;
    std::optional<SegmentHit> hit;
    float hitHeight = 0.0F;
    bool isInContact = false;
};

float Approach(float value, float target, float step) {
    return value < target ? std::min(value + step, target) : std::max(value - step, target);
}

void CastAxleRays(std::array<AxleRay, 2>& rays, const CollisionQuery& query) {
    for (std::size_t world = 0; world < query.WorldCount(); ++world) {
        for (AxleRay& ray : rays) {
            if (!ray.hit) {
                ray.hit = query.CastSegment(world, ray.start, ray.end);
                if (ray.hit) {
                    ray.hitHeight = ray.hit->point.z - ray.end.z;
                }
            }
        }
    }
}

void CopyToWheels(BodyMotion& motion, const std::array<AxleRay, 2>& rays) {
    for (std::size_t i = 0; i < kWheelCount; ++i) {
        const AxleRay& ray = rays.at(i / 2);
        WheelProbe& wheel = motion.wheels.at(i);
        wheel.hasHit = ray.hit.has_value();
        wheel.isInContact = ray.isInContact;
        wheel.contactForce = {};
        wheel.hitHeight = ray.hitHeight;
        if (ray.hit) {
            wheel.hitPoint = ray.hit->point;
            wheel.hitNormal = ray.hit->normal;
            wheel.hitSurface = ray.hit->surface;
        } else {
            wheel.hitSurface = {};
        }
    }
}

std::array<AxleRay, 2> AimAxleRays(const HoverState& hover, float sweepHeight,
                                   float sweepDistance) {
    std::array<AxleRay, 2> rays{};
    const std::array<Vector3, 2> axles{hover.frontAxle, hover.rearAxle};
    for (std::size_t i = 0; i < rays.size(); ++i) {
        rays.at(i).start = axles.at(i) + Vector3{.z = sweepHeight};
        rays.at(i).end = axles.at(i) - Vector3{.z = sweepDistance};
    }
    return rays;
}

void MarkContacts(BodyMotion& motion, std::array<AxleRay, 2>& rays, float highestHeight) {
    const float limit = std::max(0.0F, highestHeight - kContactBand);
    motion.supportNormal = {};
    for (AxleRay& ray : rays) {
        ray.isInContact = ray.hit && ray.hitHeight >= limit;
        if (ray.isInContact && ray.hit) {
            ++motion.contactCount;
            motion.supportNormal += ray.hit->normal;
        }
    }
    const std::optional<SegmentHit>& front = rays[0].hit;
    const std::optional<SegmentHit>& rear = rays[1].hit;
    if (motion.contactCount >= 2 && front && rear) {
        motion.basis = OrthonormalBasis(front->point - rear->point, motion.basis.up);
        motion.supportNormal = motion.basis.up;
    } else {
        motion.supportNormal = Normalized(motion.supportNormal, motion.basis.up);
    }
}

bool BouncesOnLanding(BodyMotion& motion, std::array<AxleRay, 2>& rays) {
    const float into = Dot(motion.velocity, motion.supportNormal);
    if (into >= 0.0F) {
        return false;
    }
    if (into < kBounceSpeed) {
        motion.velocity.z -= kBounceScale * into;
        motion.contactCount = 0;
        for (AxleRay& ray : rays) {
            ray.isInContact = false;
        }
        return true;
    }
    motion.velocity.z -= into;
    return false;
}

}

void UpdateHoverLift(HoverLift& lift, int dtMs) {
    const auto dt = static_cast<float>(dtMs);
    lift.height = Approach(lift.height, lift.targetHeight, lift.ratePerSecond * dt / kMsPerSecond);
    lift.bank = Approach(lift.bank, lift.targetBank, dt * kBankRatePerMs);
}

float HoverBankTarget(float turnRadius) {
    if (turnRadius == 0.0F) {
        return 0.0F;
    }
    const float straightness = std::min(std::abs(turnRadius) / kBankReferenceRadius, 1.0F);
    return std::copysign((1.0F - straightness) * kMaxBank, -turnRadius);
}

void UpdateHoverContact(BodyState& body, const BodyConstants& constants, int dtMs,
                        const CollisionQuery& world) {
    BodyMotion& motion = body.motion;
    if (body.derived.isAtRest && motion.velocity == Vector3{} &&
        body.angularVelocity == Vector3{}) {
        return;
    }
    body.derived.isAtRest = false;
    HoverState& hover = body.hover;
    PlaceWheels(motion, constants);
    hover.frontAxle = (motion.wheels[0].position + motion.wheels[1].position) * 0.5F;
    hover.rearAxle = (motion.wheels[2].position + motion.wheels[3].position) * 0.5F;
    const bool wasAirborne = !IsGrounded(motion);
    const float drop = std::max(0.0F, body.saved.position.z - motion.position.z);
    const float sweepDistance =
        (wasAirborne ? 0.0F : static_cast<float>(dtMs) * kFollowRatePerMs) + hover.lift.height;
    std::array<AxleRay, 2> rays = AimAxleRays(hover, constants.boundsRadius + drop, sweepDistance);
    CastAxleRays(rays, world);
    motion.contactCount = 0;
    const auto highest = std::ranges::max_element(
        rays, {}, [](const AxleRay& ray) { return ray.hit ? ray.hitHeight : -1.0F; });
    if (!highest->hit) {
        if (!wasAirborne) {
            motion.velocity.z -= kLiftOffDrop;
        }
        CopyToWheels(motion, rays);
        return;
    }
    MarkContacts(motion, rays, highest->hitHeight);
    const Vector3& selectedAxle = highest == rays.begin() ? hover.frontAxle : hover.rearAxle;
    const Vector3 delta = highest->hit->point - selectedAxle + Vector3{.z = hover.lift.height};
    motion.position += delta;
    hover.frontAxle += delta;
    hover.rearAxle += delta;
    const bool bounces = wasAirborne && BouncesOnLanding(motion, rays);
    CopyToWheels(motion, rays);
    if (!bounces) {
        const float gravityForce = -kGravity * constants.massScale;
        const Vector3& normal = motion.supportNormal;
        body.supportForce = (Vector3{.z = 1.0F} - (normal * normal.z)) * gravityForce;
    }
}

}
