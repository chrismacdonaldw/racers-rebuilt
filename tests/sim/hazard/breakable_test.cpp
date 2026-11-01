#include "sim/hazard/breakable.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "assets/surface_table.hpp"
#include "hazard_rig.hpp"

namespace rr::sim {
namespace {

using enum EventEdge;

constexpr assets::EventId kTrigger = 7;

std::vector<assets::SurfaceEntry> Surfaces() {
    return {{.name = "shootme"}};
}

assets::Hazard Props() {
    return {.kind = assets::HazardKind::BreakableProp,
            .payload = assets::BreakableProp{
                .surface = "shootme", .entities = {"block", "barrel"}, .trigger = kTrigger}};
}

TEST_CASE("the props stand frozen and the surface solid until the trigger") {
    test::HazardRig rig({Props()}, test::StillEntities({"block", "barrel"}, {}));
    rig.UseSurfaces(TrackSurfaces(Surfaces()));

    rig.Run(Milliseconds{1000});

    CHECK(rig.Entities().IsFrozen(0));
    CHECK(rig.Entities().IsFrozen(1));
    CHECK(rig.Entities().Frame(1) == 0.0F);
    CHECK(rig.Surfaces().IsSolid(0));
}

TEST_CASE("breaking props open the surface at once and fire the break event") {
    test::HazardRig rig({Props()}, test::StillEntities({"block", "barrel"}, {}));
    rig.UseSurfaces(TrackSurfaces(Surfaces()));
    rig.Hear(kTrigger, Start);
    CHECK(rig.CountStarts(kPropBreakEvent) == 1);
    rig.Hear(kTrigger, End);

    rig.Run(test::kRigTick);

    CHECK(rig.Hazards().Slots()[0].phase == HazardPhase::Active);
    CHECK_FALSE(rig.Surfaces().IsSolid(0));
}

TEST_CASE("broken props play out once and leave the surface open for good") {
    test::HazardRig rig({Props()}, test::StillEntities({"block", "barrel"}, {}));
    rig.UseSurfaces(TrackSurfaces(Surfaces()));
    rig.Hear(kTrigger, Start);

    rig.Run(Milliseconds{5000});

    CHECK(std::get<Breakable>(rig.Hazards().Slots()[0].state).isBroken);
    CHECK(rig.Hazards().Slots()[0].phase == HazardPhase::Loaded);
    CHECK(rig.Entities().IsHoldingLastFrame(1));
    CHECK(rig.Surfaces().LetsShotsThrough(0));
}

}
}
