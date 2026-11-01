#include "sim/hazard/hazard_box.hpp"

#include <algorithm>

#include "sim/physics/racer_contacts.hpp"
#include "sim/physics/rigid_body.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kBoxNode = 1;
constexpr float kBoxMass = 20'000.0F;

}

void HazardBox::Tick(HazardContext& context) {
    if (!entity) {
        return;
    }
    const Transform node = context.entities->NodeWorld(*entity, kBoxNode);
    box.centre = node.position;
    box.axes = followsNode ? ToBasis(node.rotation) : Basis{};
}

void HazardBox::Touch(HazardContext& context) const {
    if (!entity) {
        return;
    }
    BodyState body;
    body.motion.position = box.centre;
    body.motion.basis = box.axes;
    const ContactBody hazard{.body = &body, .constants = &constants};
    const BoundingSphere bounds{.centre = box.centre, .radius = constants.boundsRadius};
    for (const ContactBody& racer : context.bodies) {
        if (!racer.isSolid || racer.boxScale <= 0.0F ||
            !Touches(
                {.centre = racer.body->motion.position, .radius = racer.constants->boundsRadius},
                bounds)) {
            continue;
        }
        if (const std::optional<BoxContact> contact =
                TestBoxes(BoxOf(*racer.body, *racer.constants, racer.boxScale), box)) {
            RespondToContact(racer, hazard, *contact);
            body.motion.velocity = {};
        }
    }
}

HazardBox LoadHazardBox(const assets::SwingingBox& payload, TrackEntities& entities) {
    HazardBox hazard{.entity = entities.Find(payload.entity),
                     .followsNode = payload.entity.starts_with("rk"),
                     .box = {.halfSize = payload.size * 0.5F}};
    hazard.constants.mass = kBoxMass;
    hazard.constants.inertia = BoxInertia(kBoxMass, payload.size);
    hazard.constants.boundsRadius =
        std::max({payload.size.x, payload.size.y, payload.size.z}) * 0.5F;
    if (hazard.entity) {
        entities.Seek(*hazard.entity, payload.startFrame);
    }
    return hazard;
}

}
