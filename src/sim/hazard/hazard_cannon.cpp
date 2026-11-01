#include "sim/hazard/hazard_cannon.hpp"

#include <cstdint>
#include <span>

#include "sim/powerup/shot_world.hpp"

namespace rr::sim {
namespace {

constexpr BallisticSpec kCannonShot{
    .speed = 180.0F, .gravity = -32.176F, .lifetime = Milliseconds{3000}};
constexpr std::uint32_t kRandomRange = 1024;

const assets::AimPoint& Pick(std::span<const assets::AimPoint> points, bool atRandom,
                             Random& random) {
    if (!atRandom) {
        return points.front();
    }
    return points[random.Below(kRandomRange) % points.size()];
}

}

bool HazardCannon::StartsOn(const TrackEvent& event) const {
    if (event.id != trigger || event.edge != EventEdge::Start || launches.empty() ||
        targets.empty()) {
        return false;
    }
    if (!triggerCentre || !event.position) {
        return true;
    }
    const Vector3 offset = *event.position - *triggerCentre;
    return Dot(offset, offset) < triggerRadius * triggerRadius;
}

void HazardCannon::Activate(HazardContext& context) {
    const assets::AimPoint& launch = Pick(launches, picksAtRandom, *context.random);
    target = Pick(targets, picksAtRandom, *context.random);
    context.events->FireStartAndEnd(launch.event, launch.position);
    flight.LaunchAtPoint(kCannonShot, launch.position, target.position);
    isFlying = true;
    context.events->FireStartAndEnd(kCannonFiredEvent, launch.position);
}

bool HazardCannon::Tick(HazardContext& context) {
    if (!isFlying) {
        return true;
    }
    const ShotWorld shots{*context.world, context.surfaces, context.events};
    const FlightState state = flight.Step(context.elapsed, shots);
    if (state == FlightState::Flying) {
        return true;
    }
    isFlying = false;
    if (state != FlightState::HitWorld) {
        return true;
    }
    context.explosions->Spawn(flight.Position(), false, std::nullopt);
    context.events->FireStartAndEnd(kCannonLandedEvent, flight.Position());
    context.events->FireStartAndEnd(target.event, target.position);
    return false;
}

HazardCannon LoadCannon(const assets::CannonShot& payload) {
    HazardCannon cannon{.launches = {{.position = payload.launch}},
                        .trigger = payload.event,
                        .triggerCentre = payload.triggerCentre,
                        .triggerRadius = payload.triggerRadius};
    if (payload.target) {
        cannon.targets.push_back({.position = *payload.target});
    }
    return cannon;
}

HazardCannon LoadShipCannon(const assets::ShipCannons& payload) {
    HazardCannon cannon = LoadCannon(payload.shot);
    cannon.launches = payload.launches;
    cannon.targets = payload.targets;
    cannon.picksAtRandom = true;
    return cannon;
}

}
