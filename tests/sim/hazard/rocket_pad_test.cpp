#include "sim/hazard/rocket_pad.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "hazard_rig.hpp"

namespace rr::sim {
namespace {

constexpr Vector3 kPad{.x = 200.0F};
constexpr Vector3 kNearPad{.x = 30.0F, .z = 0.2F};

assets::Hazard RocketHazard() {
    return {.kind = assets::HazardKind::RocketPad};
}

TrackEntitySources PadModels() {
    TrackEntitySources sources;
    sources.statics.push_back({.name = "mmrocof", .placement = {.position = kPad}});
    return sources;
}

TrackSurfaces RocketSurface() {
    const std::vector<assets::SurfaceEntry> entries{{.name = "road"}, {.name = "mmrocc"}};
    return TrackSurfaces(entries);
}

std::vector<bool> Hidden(const test::HazardRig& rig) {
    return rig.Hazards().Models().Hidden();
}

void Step(test::HazardRig& rig) {
    rig.Tick();
    rig.Touch();
}

}

TEST_CASE("the rocket pad loads switched off") {
    const test::HazardRig rig({RocketHazard()}, PadModels());

    CHECK(rig.Hazards().Models().Names()[0] == "mmrocof");
    CHECK(Hidden(rig) == std::vector{false, true});
}

TEST_CASE("a shielded racer near the pad lets everyone through the rocket on the next tick") {
    test::HazardRig rig({RocketHazard()}, PadModels());
    rig.UseSurfaces(RocketSurface());
    rig.Hear(kRocketPadTrigger, EventEdge::Start);
    rig.Place(0, kNearPad);
    rig.Car(0).RaiseShield(1);

    Step(rig);
    CHECK(rig.Surfaces().IsSolid(1));
    Step(rig);

    CHECK(rig.CountStarts(kRocketPadEvent) == 1);
    CHECK(Hidden(rig) == std::vector{true, false});
    CHECK_FALSE(rig.Surfaces().IsSolid(1));
    CHECK(rig.Surfaces().LetsShotsThrough(1));
    CHECK(rig.Surfaces().IsSolid(0));
}

TEST_CASE("the rocket closes on the first tick without a shielded racer") {
    test::HazardRig rig({RocketHazard()}, PadModels());
    rig.UseSurfaces(RocketSurface());
    rig.Hear(kRocketPadTrigger, EventEdge::Start);
    rig.Place(0, kNearPad);
    rig.Car(0).RaiseShield(1);
    Step(rig);
    Step(rig);

    rig.Car(0).LowerShield();
    Step(rig);
    Step(rig);

    CHECK(Hidden(rig) == std::vector{false, true});
    CHECK(rig.Surfaces().IsSolid(1));
    CHECK(rig.Events().back() == TrackEvent{.id = kRocketPadEvent, .edge = EventEdge::End});
}

TEST_CASE("an unshielded racer leaves the rocket shut") {
    test::HazardRig rig({RocketHazard()}, PadModels());
    rig.UseSurfaces(RocketSurface());
    rig.Hear(kRocketPadTrigger, EventEdge::Start);
    rig.Place(0, kNearPad);

    Step(rig);
    Step(rig);

    CHECK(rig.Events().empty());
    CHECK(rig.Surfaces().IsSolid(1));
}

TEST_CASE("stopping the pad shuts the rocket") {
    test::HazardRig rig({RocketHazard()}, PadModels());
    rig.UseSurfaces(RocketSurface());
    rig.Hear(kRocketPadTrigger, EventEdge::Start);
    rig.Place(0, kNearPad);
    rig.Car(0).RaiseShield(1);
    Step(rig);
    Step(rig);

    rig.Hear(kRocketPadTrigger, EventEdge::End);

    CHECK(Hidden(rig) == std::vector{false, true});
    CHECK(rig.Surfaces().IsSolid(1));
}

}
