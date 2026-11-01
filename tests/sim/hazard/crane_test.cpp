#include "sim/hazard/crane.hpp"

#include <doctest/doctest.h>

#include "hazard_rig.hpp"

namespace rr::sim {
namespace {

constexpr Vector3 kBall{.x = 30.0F, .z = 2.0F};
constexpr Vector3 kUnderBall{.x = 29.0F, .z = 0.2F};

assets::Hazard CraneHazard() {
    return {.kind = assets::HazardKind::Crane};
}

TEST_CASE("the wrecking ball spins a moving racer it touches") {
    test::HazardRig rig({CraneHazard()}, test::StillEntities({"crane"}, kBall), 2);
    rig.Hear(kCraneTrigger, EventEdge::Start);
    rig.Place(0, kUnderBall);
    rig.Drive(5);

    rig.Tick();
    rig.Touch();

    CHECK(rig.Car(0).IsSpinning());
    CHECK_FALSE(rig.Car(1).IsSpinning());
    CHECK(rig.CountStarts(kCraneHitEvent) == 1);
}

TEST_CASE("the wrecking ball leaves stopped and spinning racers alone") {
    test::HazardRig rig({CraneHazard()}, test::StillEntities({"crane"}, kBall), 2);
    rig.Hear(kCraneTrigger, EventEdge::Start);
    rig.Place(0, kUnderBall);
    rig.Drive(5);
    rig.Car(0).StartSpin(1.0F, 0.01F);
    rig.Place(1, kUnderBall);

    rig.Tick();
    rig.Touch();

    CHECK(rig.CountStarts(kCraneHitEvent) == 0);
    CHECK_FALSE(rig.Car(1).IsSpinning());
}

TEST_CASE("the crane swings once a strike window comes round") {
    test::HazardRig rig({CraneHazard()}, test::StillEntities({"crane"}, kBall));
    rig.Hear(kCraneTrigger, EventEdge::Start);

    rig.Seek(0, 10.0F);
    rig.Tick();
    CHECK(rig.CountStarts(kCraneSwingEvent) == 1);
    rig.Seek(0, 160.0F);
    rig.Tick();
    CHECK(rig.CountStarts(kCraneSwingEvent) == 0);
}

TEST_CASE("a crane that hasn't started touches nobody") {
    test::HazardRig rig({CraneHazard()}, test::StillEntities({"crane"}, kBall));
    rig.Place(0, kUnderBall);
    rig.Drive(5);

    rig.Tick();
    rig.Touch();

    CHECK_FALSE(rig.Car(0).IsSpinning());
}

}
}
