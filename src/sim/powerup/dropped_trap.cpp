#include "sim/powerup/dropped_trap.hpp"

#include <optional>

#include "core/bounding_sphere.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr Vector3 kDropRay{.z = -50.0F};
constexpr Vector3 kAboveGround{.z = 1.0F};
constexpr float kDropKickRate = 0.0015F;
constexpr Milliseconds kDropKick{150};

}

DropPoint DropUnder(const Racer& owner, const CollisionQuery& world) {
    const Vector3& position = owner.Body().motion.position;
    if (const std::optional<SegmentHit> hit = CastSolid(world, position, position + kDropRay)) {
        return {.position = hit->point + kAboveGround, .normal = hit->normal};
    }
    return {.position = position};
}

void KickOwnerOnDrop(Racer& owner) {
    owner.KickPitch(kDropKickRate, kDropKick);
}

bool IsTouching(const PowerupContext& context, const Trap& trap, std::size_t racer) {
    return racer != trap.owner && !context.racers[racer].Status().isGhost &&
           Touches({.centre = trap.position, .radius = trap.radius}, context.reaches[racer]);
}

bool RunsOut(Milliseconds& remaining, Milliseconds elapsed) {
    if (remaining <= elapsed) {
        remaining = Milliseconds{0};
        return true;
    }
    remaining -= elapsed;
    return false;
}

}
