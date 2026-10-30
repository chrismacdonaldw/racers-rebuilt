#include "sim/race/respawn.hpp"

#include <cmath>
#include <cstddef>
#include <span>

namespace rr::sim {
namespace {

constexpr Vector3 kDropStart{.z = 2.0F};
constexpr Vector3 kDropEnd{.z = -500.0F};
constexpr float kFallDepth = 20.0F;

std::optional<Vector3> GroundBelow(const Vector3& point, const CollisionQuery& world) {
    std::optional<SegmentHit> nearest;
    for (std::size_t i = 0; i < world.WorldCount(); ++i) {
        const std::optional<SegmentHit> hit =
            world.CastSegment(i, point + kDropStart, point + kDropEnd);
        if (hit && hit->isSolid && (!nearest || hit->fraction < nearest->fraction)) {
            nearest = hit;
        }
    }
    return nearest.transform([](const SegmentHit& hit) { return hit.point; });
}

float HeadingThrough(const assets::Checkpoint& gate) {
    return std::atan2(-gate.normal.y, -gate.normal.x);
}

}

bool RespawnWatch::Update(const RespawnRule& rule, Milliseconds elapsed, bool isGrounded, float z,
                          float lowestTrackZ) {
    airborne_ = isGrounded ? Milliseconds{0} : airborne_ + elapsed;
    if (!rule.isEnabled || (airborne_ < rule.timeout && z >= lowestTrackZ - kFallDepth)) {
        return false;
    }
    airborne_ = Milliseconds{0};
    return true;
}

RacerStart RespawnPoint(const CheckpointGraph& graph, std::optional<std::size_t> lastCheckpoint,
                        const RacerStart& gridSlot, const CollisionQuery& world) {
    const std::span<const assets::Checkpoint> checkpoints = graph.Checkpoints();
    std::size_t gate = lastCheckpoint.value_or(0);
    for (std::size_t step = 0; lastCheckpoint && step < checkpoints.size(); ++step) {
        const assets::Checkpoint& checkpoint = checkpoints[gate];
        gate = checkpoint.links[0];
        if (const std::optional<Vector3> ground = GroundBelow(checkpoint.position, world)) {
            return {.position = *ground, .headingRadians = HeadingThrough(checkpoints[gate])};
        }
    }
    return {.position = GroundBelow(gridSlot.position, world).value_or(gridSlot.position),
            .headingRadians = gridSlot.headingRadians};
}

}
