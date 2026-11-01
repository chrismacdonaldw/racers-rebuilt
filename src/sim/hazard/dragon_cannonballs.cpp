#include "sim/hazard/dragon_cannonballs.hpp"

#include <cstdint>

#include "core/assert.hpp"
#include "sim/physics/rigid_body.hpp"
#include "sim/powerup/powerup_system.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kEmitterNode = 1;
constexpr float kEmitterBack = 35.0F;
constexpr std::uint32_t kOffsets = 4;
constexpr float kOffsetBias = 2.0F;
constexpr float kDropHeight = 1.0F;
constexpr std::uint8_t kCannonballLevel = 0;

float Offset(Random& random) {
    return static_cast<float>(random.Below(kRollRange) % kOffsets) - kOffsetBias;
}

}

void DragonCannonballs::Tick(HazardContext& context) {
    RR_ASSERT(context.random != nullptr && context.effects != nullptr && context.aim != nullptr);
    wait += context.elapsed;
    if (wait < interval || !entity) {
        return;
    }
    wait = Milliseconds{0};
    const Transform node = context.entities->NodeWorld(*entity, kEmitterNode);
    const Vector3 emitter = node.position - (ToBasis(node.rotation).forward * kEmitterBack);
    Vector3 drop{.x = Offset(*context.random), .y = Offset(*context.random), .z = kDropHeight};
    if (drop.x == 0.0F && drop.y == 0.0F) {
        drop.x = kOffsetBias;
    }
    *context.aim = {
        .source = emitter, .target = emitter + drop, .direction = Normalized(drop, {.z = 1.0F})};
    context.effects->powerups->Fire(
        {.colour = assets::BrickColour::Red, .level = kCannonballLevel, .hazard = context.slot},
        *context.effects);
}

DragonCannonballs LoadDragonCannonballs(const assets::DragonCannonballs& payload,
                                        const TrackEntities& entities) {
    return {.entity = entities.Find(payload.entity), .interval = payload.interval};
}

}
