#include "sim/hazard/rocket_pad.hpp"

#include "sim/collision/track_surfaces.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr float kPadRadius = 350.0F;
constexpr std::string_view kSurface = "mmrocc";

}

void RocketPad::Tick(HazardContext& context) {
    if (hasShieldedRacer && !isOn) {
        Switch(true, context);
    } else if (!hasShieldedRacer && isOn) {
        Switch(false, context);
    }
    hasShieldedRacer = false;
}

void RocketPad::Touch(HazardContext& context) {
    if (!reach) {
        return;
    }
    for (std::size_t i = 0; i < context.racers.size(); ++i) {
        if (context.racers[i].IsShielded() && Touches(*reach, context.reaches[i])) {
            hasShieldedRacer = true;
        }
    }
}

void RocketPad::Deactivate(HazardContext& context) {
    if (isOn) {
        Switch(false, context);
    }
}

void RocketPad::Switch(bool on, HazardContext& context) {
    isOn = on;
    context.models->SetHidden(onModel, !on);
    context.models->SetHidden(offModel, on);
    if (const std::optional<std::size_t> surface = context.surfaces->Find(kSurface)) {
        context.surfaces->SetPassable(*surface, on);
    }
    context.events->Fire({.id = kRocketPadEvent, .edge = on ? EventEdge::Start : EventEdge::End});
}

RocketPad LoadRocketPad(const TrackEntities& entities, ModelSwitches& models) {
    RocketPad pad{.offModel = models.Add("mmrocof"), .onModel = models.Add("mmrocon")};
    models.SetHidden(pad.onModel, true);
    if (const std::optional<Transform> placement = entities.FindStatic("mmrocof")) {
        pad.reach = BoundingSphere{.centre = placement->position, .radius = kPadRadius};
    }
    return pad;
}

}
