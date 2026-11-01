#include "sim/hazard/falling_pillar.hpp"

namespace rr::sim {
namespace {

constexpr float kLandingFrame = 50.0F;

}

void FallingPillar::Activate(HazardContext& context) {
    if (entity) {
        context.entities->Play(*entity, 0, false);
    }
    hasLanded = false;
}

void FallingPillar::Tick(HazardContext& context) {
    if (hasLanded || !entity || context.entities->Frame(*entity) <= kLandingFrame) {
        return;
    }
    if (const std::optional<std::size_t> surface = context.surfaces->Find("pilcol")) {
        context.surfaces->SetPassable(*surface, false);
    }
    context.events->FireStartAndEnd(kPillarLandedEvent, std::nullopt);
    hasLanded = true;
}

FallingPillar LoadFallingPillar(TrackEntities& entities) {
    const std::optional<std::size_t> entity = entities.Find("piltop");
    if (entity) {
        entities.Freeze(*entity);
    }
    return {.entity = entity};
}

}
