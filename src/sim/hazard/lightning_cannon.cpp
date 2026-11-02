#include "sim/hazard/lightning_cannon.hpp"

#include <cmath>
#include <cstdint>
#include <numbers>

#include "core/assert.hpp"
#include "core/mirror.hpp"
#include "sim/powerup/powerup_system.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kRootNode = 0;
constexpr Vector3 kBelowRoot{.z = 17.0F};
constexpr float kDipsPerPeriod = 4.0F;
constexpr std::uint8_t kLightningLevel = 2;

float Ms(Milliseconds time) {
    return static_cast<float>(time.count());
}

}

void LightningCannon::Tick(HazardContext& context) {
    RR_ASSERT(context.effects != nullptr && context.aim != nullptr);
    sweepTime += context.elapsed;
    if (sweepTime >= period) {
        sweepTime = Milliseconds{0};
    }
    const float share = period > Milliseconds{0} ? Ms(sweepTime) / Ms(period) : 0.0F;
    const float swept = sweepRadians * share * 2.0F;
    const float yaw = share > 0.5F ? std::numbers::pi_v<float> - swept : swept;
    const float dip =
        (std::cos(2.0F * std::numbers::pi_v<float> * kDipsPerPeriod * share) * verticalRange) +
        verticalBase;
    const Vector3 direction =
        MirroredYIf(context.isMirrored,
                    Normalized({.x = std::cos(yaw), .y = std::sin(yaw), .z = dip}, {.x = 1.0F}));
    const Vector3 from =
        entity ? context.entities->NodeWorld(*entity, kRootNode).position - kBelowRoot : source;
    *context.aim = {.source = from, .target = from + direction, .direction = direction};
    cooldown += context.elapsed;
    if (cooldown >= kLightningCannonCooldown) {
        cooldown = Milliseconds{0};
        context.effects->powerups->Fire(
            {.colour = assets::BrickColour::Red, .level = kLightningLevel, .hazard = context.slot},
            *context.effects);
    }
}

LightningCannon LoadLightningCannon(const assets::LightningCannon& payload,
                                    const TrackEntities& entities) {
    LightningCannon cannon{.source = payload.source.value_or(Vector3{}),
                           .period = payload.period,
                           .sweepRadians = payload.sweepRadians,
                           .verticalRange = payload.verticalRange,
                           .verticalBase = payload.verticalBase};
    if (payload.entity) {
        cannon.entity = entities.Find(*payload.entity);
        if (const std::optional<Transform> placed = entities.FindStatic(*payload.entity)) {
            cannon.source = placed->position - kBelowRoot;
        }
    }
    return cannon;
}

}
