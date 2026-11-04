#include "sim/powerup/warp_landing.hpp"

#include <numbers>
#include <optional>

namespace rr::sim {
namespace {

constexpr float kAimAhead = 60.0F;
constexpr float kProbeLength = 60.0F;
constexpr Vector3 kProbeHeight{.z = 2.0F};
constexpr Vector3 kUp{.z = 1.0F};
constexpr float kGroundSlope = 0.7F;
constexpr float kTurnStep = 15.0F * std::numbers::pi_v<float> / 180.0F;
constexpr int kTurnSteps = 5;

Vector3 Flat(const Vector3& v) {
    return {.x = v.x, .y = v.y};
}

float ClearRun(const CollisionQuery& world, const Vector3& from, const Vector3& heading,
               float rise) {
    const Vector3 start = from + kProbeHeight;
    const std::optional<SegmentHit> hit =
        CastSolid(world, start, start + (heading * kProbeLength) + Vector3{.z = rise});
    return hit && hit->normal.z < kGroundSlope ? hit->fraction : 1.0F;
}

}

Vector3 ClearLandingHeading(const CollisionQuery& world, const CheckpointGraph& checkpoints,
                            const Vector3& ground, std::size_t gate) {
    const Vector3 aim = checkpoints.WalkAhead(ground, kAimAhead, gate);
    const Vector3 gateHeading =
        Normalized(Flat(-checkpoints.Checkpoints()[gate].normal), {.x = 1.0F});
    const Vector3 along = Normalized(Flat(aim - ground), gateHeading);
    const float rise = (aim.z - ground.z) * (kProbeLength / kAimAhead);
    Vector3 best = along;
    float bestRun = ClearRun(world, ground, along, rise);
    for (int step = 1; step <= kTurnSteps && bestRun < 1.0F; ++step) {
        for (const float side : {1.0F, -1.0F}) {
            const Vector3 turned =
                RotateAbout(along, kUp, side * kTurnStep * static_cast<float>(step));
            const float run = ClearRun(world, ground, turned, rise);
            if (run > bestRun) {
                best = turned;
                bestRun = run;
            }
            if (run >= 1.0F) {
                break;
            }
        }
    }
    return best;
}

}
