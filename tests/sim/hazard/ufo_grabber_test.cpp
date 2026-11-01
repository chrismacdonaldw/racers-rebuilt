#include "sim/hazard/ufo_grabber.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <variant>

#include "hazard_rig.hpp"

namespace rr::sim {
namespace {

constexpr assets::EventId kTrigger = 1;
constexpr Vector3 kBeam{.z = 20.0F};
constexpr Vector3 kUnderBeam{.x = 30.0F, .z = 0.2F};
constexpr int kTicksPerSecond = (1000 / 12) + 2;

assets::Hazard UfoHazard() {
    return {.kind = assets::HazardKind::UfoGrabber,
            .payload = assets::UfoGrabber{.trigger = kTrigger,
                                          .entity = "ufofly1",
                                          .pullStrength = 800.0F,
                                          .grabFrameLow = 85.0F,
                                          .grabFrameHigh = 200.0F}};
}

const UfoGrabber& Ufo(const test::HazardRig& rig) {
    return std::get<UfoGrabber>(rig.Hazards().Slots()[0].state);
}

const std::optional<Vector3>& Pull(test::HazardRig& rig, std::size_t racer = 0) {
    return rig.Car(racer).Body().externalForces[1];
}

void Step(test::HazardRig& rig, int ticks = 1) {
    for (int tick = 0; tick < ticks; ++tick) {
        rig.Tick();
        rig.Touch();
    }
}

}

TEST_CASE("the UFO pulls the racer under its beam toward the beam") {
    test::HazardRig rig({UfoHazard()}, test::StillEntities({"ufofly1"}, kBeam));
    rig.Hear(kTrigger, EventEdge::Start);
    rig.Place(0, kUnderBeam);

    Step(rig);

    REQUIRE(Pull(rig).has_value());
    CHECK(Pull(rig).value_or(Vector3{}).x < 0.0F);
    CHECK(Pull(rig).value_or(Vector3{}).z == 0.0F);
    CHECK(Ufo(rig).phase == GrabPhase::Holding);
}

TEST_CASE("the UFO lets go after a second and rests for another") {
    test::HazardRig rig({UfoHazard()}, test::StillEntities({"ufofly1"}, kBeam));
    rig.Hear(kTrigger, EventEdge::Start);
    rig.Place(0, kUnderBeam);

    Step(rig, kTicksPerSecond);
    CHECK_FALSE(Pull(rig).has_value());
    CHECK(Ufo(rig).phase == GrabPhase::Lockout);

    Step(rig, kTicksPerSecond);
    CHECK(Ufo(rig).phase == GrabPhase::Holding);
    CHECK(Pull(rig).has_value());
}

TEST_CASE("the UFO drops a racer that leaves the beam for half a second") {
    test::HazardRig rig({UfoHazard()}, test::StillEntities({"ufofly1"}, kBeam));
    rig.Hear(kTrigger, EventEdge::Start);
    rig.Place(0, kUnderBeam);
    Step(rig);

    rig.Place(0, {.x = 500.0F, .z = 0.2F});
    Step(rig, (500 / 12) + 1);

    CHECK_FALSE(Pull(rig).has_value());
    CHECK(Ufo(rig).phase == GrabPhase::Free);
}

TEST_CASE("the UFO holds one racer at a time and never a shielded one") {
    test::HazardRig rig({UfoHazard()}, test::StillEntities({"ufofly1"}, kBeam), 3);
    rig.Hear(kTrigger, EventEdge::Start);
    for (std::size_t i = 0; i < 3; ++i) {
        rig.Place(i, kUnderBeam);
    }
    rig.Car(0).RaiseShield(1);

    Step(rig);

    CHECK_FALSE(Pull(rig, 0).has_value());
    CHECK(Ufo(rig).held == 1);
    CHECK_FALSE(Pull(rig, 2).has_value());
}

TEST_CASE("the UFO can't grab between its low and high frames") {
    test::HazardRig rig({UfoHazard()}, test::StillEntities({"ufofly1"}, kBeam));
    rig.Hear(kTrigger, EventEdge::Start);
    rig.Place(0, kUnderBeam);
    rig.Seek(0, 90.0F);

    Step(rig);

    CHECK_FALSE(Pull(rig).has_value());
}

TEST_CASE("the UFO lets go when it stops") {
    test::HazardRig rig({UfoHazard()}, test::StillEntities({"ufofly1"}, kBeam));
    rig.Hear(kTrigger, EventEdge::Start);
    rig.Place(0, kUnderBeam);
    Step(rig);

    rig.Hear(kTrigger, EventEdge::End);

    CHECK_FALSE(Pull(rig).has_value());
    CHECK_FALSE(Ufo(rig).held.has_value());
}

}
