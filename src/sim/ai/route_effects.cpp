#include "sim/ai/route_effects.hpp"

#include "core/quaternion.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr float kMsPerSecond = 1000.0F;
constexpr float kHopGravity = -100.0F;
constexpr float kTiltReturnScale = 0.25F;

Basis BasisOf(const Quaternion& rotation) {
    return {.forward = Rotate(rotation, {.x = 1.0F}),
            .side = Rotate(rotation, {.y = 1.0F}),
            .up = Rotate(rotation, {.z = 1.0F})};
}

Vector3 RotateAbout(const Vector3& v, const Vector3& axis, float radians) {
    return Rotate(FromAxisAngle(axis, radians), v);
}

Basis Roll(const Basis& basis, float radians) {
    return OrthonormalBasis(basis.forward, RotateAbout(basis.up, basis.forward, radians));
}

Basis PitchUp(const Basis& basis, float radians) {
    return OrthonormalBasis(RotateAbout(basis.forward, basis.side, -radians),
                            RotateAbout(basis.up, basis.side, -radians));
}

bool Bank(const RouteEffects& effects, Basis& basis) {
    if (!effects.isHovering && effects.lift.bank == 0.0F) {
        return false;
    }
    basis = Roll(basis, effects.lift.bank);
    return true;
}

bool Tilt(RouteTilt& tilt, const Vector3& wheelAnchor, int dtMs, Basis& basis) {
    if (tilt.remainingMs == 0 && tilt.angle == 0.0F) {
        return false;
    }
    if (tilt.remainingMs != 0) {
        if (dtMs >= tilt.remainingMs) {
            tilt.remainingMs = 0;
            tilt.ratePerMs = -tilt.ratePerMs * kTiltReturnScale;
        } else {
            tilt.remainingMs -= dtMs;
        }
    }
    const bool wasNonNegative = tilt.angle >= 0.0F;
    tilt.angle += tilt.ratePerMs * static_cast<float>(dtMs);
    if (tilt.remainingMs == 0 && wasNonNegative != (tilt.angle >= 0.0F)) {
        tilt.angle = 0.0F;
        tilt.height = 0.0F;
        return false;
    }
    const float before = ToWorld(basis, wheelAnchor).z;
    basis = PitchUp(basis, tilt.angle);
    tilt.height = ToWorld(basis, wheelAnchor).z - before;
    return true;
}

bool Spin(RouteEffects& effects, int dtMs, Basis& basis) {
    RouteSpin& spin = effects.spin;
    if (spin.remainingMs == 0) {
        return false;
    }
    if (dtMs >= spin.remainingMs) {
        spin.remainingMs = 0;
        effects.isSpinning = false;
        return false;
    }
    spin.remainingMs -= dtMs;
    spin.angle += spin.ratePerMs * static_cast<float>(dtMs);
    basis = OrthonormalBasis(RotateAbout(basis.forward, basis.up, spin.angle), basis.up);
    return true;
}

}

void StartRouteTilt(RouteTilt& tilt, float ratePerMs, int durationMs) {
    if (tilt.remainingMs != 0) {
        return;
    }
    tilt.angle = 0.0F;
    tilt.ratePerMs = -ratePerMs;
    tilt.remainingMs = durationMs;
}

void UpdateRouteHop(RouteEffects& effects, int dtMs) {
    RouteHop& hop = effects.hop;
    if (hop.velocity == 0.0F && hop.height == 0.0F) {
        return;
    }
    const float seconds = static_cast<float>(dtMs) / kMsPerSecond;
    hop.velocity += kHopGravity * seconds;
    hop.height += seconds * hop.velocity;
    if (hop.height <= 0.0F) {
        hop = {};
        effects.isSpunOut = false;
    }
}

Transform PlaceOnRoute(RouteEffects& effects, const Transform& routePose,
                       const Vector3& wheelAnchor, int dtMs) {
    Basis basis = BasisOf(routePose.rotation);
    const bool isBanked = Bank(effects, basis);
    const bool isTilted = Tilt(effects.tilt, wheelAnchor, dtMs, basis);
    const bool isSpun = Spin(effects, dtMs, basis);
    const float lift = effects.tilt.height + effects.hop.height + effects.lift.height;
    return {.position = routePose.position + Vector3{.z = lift},
            .rotation = isBanked || isTilted || isSpun ? ToQuaternion(basis) : routePose.rotation};
}

}
