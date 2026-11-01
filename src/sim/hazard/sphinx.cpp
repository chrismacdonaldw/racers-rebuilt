#include "sim/hazard/sphinx.hpp"

#include <algorithm>

namespace rr::sim {

void Sphinx::Activate(HazardContext& context) {
    if (isDestroyed) {
        return;
    }
    if (entity) {
        context.events->Fire({.id = kSphinxBlowUpEvent,
                              .edge = EventEdge::Start,
                              .position = context.entities->Entity(*entity).placement.position});
        blowUpLeft = kSphinxBlowUp;
    }
    if (const std::optional<std::size_t> surface = context.surfaces->Find("sphinx")) {
        context.surfaces->SetPassable(*surface, true);
    }
}

bool Sphinx::Tick(HazardContext& context) {
    if (isDestroyed) {
        return false;
    }
    blowUpLeft = std::max(blowUpLeft - context.elapsed, Milliseconds{0});
    if (blowUpLeft > Milliseconds{0}) {
        return true;
    }
    if (entity) {
        context.events->Fire({.id = kSphinxBlowUpEvent,
                              .edge = EventEdge::End,
                              .position = context.entities->Entity(*entity).placement.position});
    }
    context.models->SetHidden(model, true);
    isDestroyed = true;
    return false;
}

Sphinx LoadSphinx(const TrackEntities& entities, ModelSwitches& models) {
    return {.entity = entities.Find("blowup"), .model = models.Add("blowup")};
}

}
