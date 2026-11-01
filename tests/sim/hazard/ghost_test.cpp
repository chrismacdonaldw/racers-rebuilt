#include "sim/hazard/ghost.hpp"

#include <doctest/doctest.h>

#include <variant>

#include "hazard_rig.hpp"

namespace rr::sim {
namespace {

constexpr Vector3 kGhost{.x = 30.0F, .z = 3.0F};
constexpr Vector3 kNearGhost{.x = 38.0F, .z = 0.2F};

assets::Hazard GhostHazard() {
    return {.kind = assets::HazardKind::Ghost};
}

TrackEntitySources GhostEntity() {
    TrackEntitySources sources = test::StillEntities({"ghostly"}, kGhost);
    sources.entities[0].radius = 2.0F;
    return sources;
}

}

TEST_CASE("the ghost reaches its model's radius plus 8 from its body") {
    const test::HazardRig rig({GhostHazard()}, GhostEntity());

    const auto& ghost = std::get<Ghost>(rig.Hazards().Slots()[0].state);

    CHECK(ghost.reach.radius == doctest::Approx(10.0F));
}

TEST_CASE("the ghost spins out a moving racer and hops it up") {
    test::HazardRig rig({GhostHazard()}, GhostEntity(), 2);
    rig.Hear(kGhostTrigger, EventEdge::Start);
    rig.Place(0, kNearGhost);
    rig.Drive(5);
    rig.Place(1, kNearGhost);

    rig.Tick();
    rig.Touch();

    CHECK(rig.Car(0).IsSpinning());
    CHECK(rig.Car(0).IsSpunOut());
    CHECK(rig.Car(0).Body().motion.velocity.z > 0.0F);
    CHECK(rig.Car(0).Status().hitReactions == 1);
    CHECK_FALSE(rig.Car(1).IsSpinning());
}

TEST_CASE("the ghost passes through shielded, stopped and spinning racers") {
    test::HazardRig rig({GhostHazard()}, GhostEntity(), 3);
    rig.Hear(kGhostTrigger, EventEdge::Start);
    for (std::size_t i = 0; i < 3; ++i) {
        rig.Place(i, kNearGhost);
    }
    rig.Drive(5);
    rig.Car(0).RaiseShield(1);
    rig.Car(1).StartSpin(1.0F, 0.01F);
    rig.Place(2, kNearGhost);

    rig.Tick();
    rig.Touch();

    CHECK(rig.Car(0).Status().hitReactions == 0);
    CHECK(rig.Car(1).Status().hitReactions == 0);
    CHECK(rig.Car(2).Status().hitReactions == 0);
}

TEST_CASE("a ghost that hasn't started haunts nobody") {
    test::HazardRig rig({GhostHazard()}, GhostEntity());
    rig.Place(0, kNearGhost);
    rig.Drive(5);

    rig.Tick();
    rig.Touch();

    CHECK_FALSE(rig.Car(0).IsSpinning());
}

}
