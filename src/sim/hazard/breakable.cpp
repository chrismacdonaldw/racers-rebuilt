#include "sim/hazard/breakable.hpp"

#include <algorithm>
#include <optional>

namespace rr::sim {
namespace {

void Hide(const Breakable& prop, ModelSwitches& models, bool isHidden) {
    for (const std::size_t model : prop.models) {
        models.SetHidden(model, isHidden);
    }
}

}

void Breakable::Activate(HazardContext& context) {
    for (const std::size_t entity : entities) {
        context.entities->PlayOut(entity);
    }
    Hide(*this, *context.models, false);
    isBroken = false;
    context.events->FireStartAndEnd(kPropBreakEvent, std::nullopt);
}

bool Breakable::Tick(HazardContext& context) {
    if (const std::optional<std::size_t> found = context.surfaces->Find(surface)) {
        context.surfaces->SetPassable(*found, true);
    }
    isBroken = std::ranges::all_of(
        entities, [&](std::size_t entity) { return context.entities->IsHoldingLastFrame(entity); });
    if (isBroken) {
        Hide(*this, *context.models, true);
    }
    return !isBroken;
}

Breakable LoadBreakable(const assets::BreakableProp& payload, TrackEntities& entities,
                        ModelSwitches& models) {
    Breakable prop{.surface = payload.surface};
    for (const std::string& name : payload.entities) {
        if (const std::optional<std::size_t> entity = entities.Find(name)) {
            entities.Freeze(*entity);
            prop.entities.push_back(*entity);
            prop.models.push_back(models.Add(name));
        }
    }
    return prop;
}

}
