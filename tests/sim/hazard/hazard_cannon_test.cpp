#include "sim/hazard/hazard_cannon.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <variant>

#include "hazard_rig.hpp"
#include "sim/collision/track_surfaces.hpp"
#include "sim/collision/world_list.hpp"
#include "sim/hazard/hazards.hpp"
#include "sim/powerup/explosions.hpp"
#include "sim/race/race_track.hpp"
#include "sim/race/random.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_events.hpp"

namespace rr::sim {
namespace {

using enum EventEdge;

constexpr assets::EventId kTaunt = 999;
constexpr Vector3 kLaunch{.z = 20.0F};
constexpr Vector3 kTarget{.x = 150.0F};

assets::Hazard Cannon(const Vector3& target = kTarget) {
    return {.kind = assets::HazardKind::Cannon,
            .payload = assets::CannonShot{.launch = kLaunch,
                                          .target = target,
                                          .triggerCentre = Vector3{},
                                          .triggerRadius = 50.0F,
                                          .event = kTaunt}};
}

HazardPhase PhaseOf(const test::HazardRig& rig) {
    return rig.Hazards().Slots()[0].phase;
}

std::optional<Vector3> StartOf(const test::HazardRig& rig, assets::EventId id) {
    const auto event = std::ranges::find_if(rig.Events(), [&](const TrackEvent& fired) {
        return fired.id == id && fired.edge == Start;
    });
    return event == rig.Events().end() ? std::nullopt : event->position;
}

std::optional<Vector3> RunUntilLanded(test::HazardRig& rig) {
    for (Milliseconds time{0}; time < Milliseconds{3000}; time += test::kRigTick) {
        rig.Tick();
        if (const std::optional<Vector3> landed = StartOf(rig, kCannonLandedEvent)) {
            return landed;
        }
    }
    return std::nullopt;
}

Milliseconds TickUntilLanded(Hazards& hazards, HazardContext& context) {
    const auto hasLanded = [&] {
        return std::ranges::any_of(context.events->Events(), [](const TrackEvent& event) {
            return event.id == kCannonLandedEvent;
        });
    };
    Milliseconds time{0};
    do {
        context.events->BeginTick();
        hazards.Tick(context);
        time += test::kRigTick;
    } while (!hasLanded() && time < Milliseconds{3000});
    return time;
}

TEST_CASE("the cannon fires only on its trigger event inside the trigger radius") {
    test::HazardRig rig({Cannon()}, {});

    rig.Hear(kTaunt, Start, Vector3{.x = 60.0F});
    CHECK(PhaseOf(rig) == HazardPhase::Loaded);
    rig.Hear(kTaunt + 1, Start, Vector3{});
    CHECK(PhaseOf(rig) == HazardPhase::Loaded);
    rig.Hear(kTaunt, Start, Vector3{.x = 40.0F});

    CHECK(PhaseOf(rig) == HazardPhase::Active);
    CHECK(StartOf(rig, kCannonFiredEvent) == kLaunch);
}

TEST_CASE("the ball lands at its target and the cannon reloads") {
    test::HazardRig rig({Cannon()}, {});
    rig.Hear(kTaunt, Start, Vector3{});

    const std::optional<Vector3> landed = RunUntilLanded(rig);

    REQUIRE(landed);
    CHECK(Length(*landed - kTarget) < 2.0F);
    CHECK(PhaseOf(rig) == HazardPhase::Loaded);
}

TEST_CASE("the ball lands in an ownerless blast without a scar") {
    test::HazardRig rig({Cannon()}, {});
    rig.Hear(kTaunt, Start);
    const std::optional<Vector3> landed = RunUntilLanded(rig);

    const auto blast = std::ranges::find_if(rig.Blasts().All(), [](const Explosion& explosion) {
        return explosion.phase == ExplosionPhase::Exploding;
    });

    REQUIRE(blast != rig.Blasts().All().end());
    CHECK(blast->position == landed);
    CHECK(blast->owner == std::nullopt);
    CHECK_FALSE(blast->leavesScar);
}

TEST_CASE("a ball that runs out without landing jams the cannon for good") {
    test::HazardRig rig({Cannon({.x = 100.0F, .z = 500.0F})}, {});
    rig.Hear(kTaunt, Start);

    CHECK_FALSE(RunUntilLanded(rig));
    rig.Tick();
    rig.Hear(kTaunt, Start);

    CHECK(PhaseOf(rig) == HazardPhase::Active);
    CHECK_FALSE(StartOf(rig, kCannonFiredEvent));
}

TEST_CASE("ship cannons draw a launch and a target, then fire the target's event there") {
    const std::vector<assets::AimPoint> launches{{.position = {.y = 10.0F, .z = 20.0F}},
                                                 {.position = {.y = -10.0F, .z = 20.0F}}};
    const std::vector<assets::AimPoint> targets{{.position = {.x = 100.0F}, .event = 8},
                                                {.position = {.x = 140.0F}, .event = 50},
                                                {.position = {.x = 180.0F}, .event = 8}};
    test::HazardRig rig(
        {{.kind = assets::HazardKind::ShipCannons,
          .payload = assets::ShipCannons{.launches = launches,
                                         .targets = targets,
                                         .shot = {.triggerRadius = 700.0F, .event = 96}}}},
        {});
    Random twin(1);
    const std::uint32_t launch = twin.Below(1024) % launches.size();
    const std::uint32_t target = twin.Below(1024) % targets.size();

    rig.Hear(96, Start);
    CHECK(StartOf(rig, kCannonFiredEvent) == launches[launch].position);
    REQUIRE(RunUntilLanded(rig));

    CHECK(StartOf(rig, targets[target].event) == targets[target].position);
}

TEST_CASE("the retail cannon's ball lands just past its 1.56 s aim and breaks the barricade") {
    const std::optional<RaceTrack> track = test::RetailRaceTrack("RACEC0R1");
    if (!track) {
        return;
    }
    TrackEntities entities(track->entities);
    TrackSurfaces surfaces(track->collision.surfaces);
    Random random(1);
    Hazards hazards(track->hazards, entities, random);
    TrackEvents events({}, 0, false, random, 1 + Hazards::MostEventsPerTick(track->hazards, 0));
    const WorldList world(track->collision.Track());
    Explosions explosions;
    HazardContext context{.entities = &entities,
                          .events = &events,
                          .surfaces = &surfaces,
                          .models = &hazards.Models(),
                          .random = &random,
                          .elapsed = test::kRigTick,
                          .world = &world,
                          .explosions = &explosions};
    events.Fire({.id = kTaunt, .edge = Start});
    hazards.Hear(events, context);

    const Milliseconds time = TickUntilLanded(hazards, context);
    hazards.Hear(events, context);

    CHECK(time >= Milliseconds{1558});
    CHECK(time < Milliseconds{1700});
    const auto prop = std::ranges::find_if(hazards.Slots(), [](const HazardSlot& slot) {
        return std::holds_alternative<Breakable>(slot.state);
    });
    REQUIRE(prop != hazards.Slots().end());
    CHECK(prop->phase == HazardPhase::Active);
}

}
}
