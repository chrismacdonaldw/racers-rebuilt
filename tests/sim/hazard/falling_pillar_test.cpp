#include "sim/hazard/falling_pillar.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "assets/surface_table.hpp"
#include "hazard_rig.hpp"

namespace rr::sim {
namespace {

using enum EventEdge;

std::vector<assets::SurfaceEntry> Surfaces() {
    return {{.name = "pilcol", .isNonSolid = true, .isProjectilePassable = true}};
}

std::size_t CountLandings(test::HazardRig& rig, Milliseconds time) {
    std::size_t landings = 0;
    for (Milliseconds run{0}; run < time; run += test::kRigTick) {
        rig.Run(test::kRigTick);
        landings += rig.CountStarts(kPillarLandedEvent);
    }
    return landings;
}

TEST_CASE("the pillar stands frozen until its trigger starts") {
    test::HazardRig rig({{.kind = assets::HazardKind::FallingPillar}},
                        test::StillEntities({"piltop"}, {}));
    rig.UseSurfaces(TrackSurfaces(Surfaces()));

    rig.Run(Milliseconds{1000});

    CHECK(rig.Entities().IsFrozen(0));
    CHECK(rig.Entities().Frame(0) == 0.0F);
}

TEST_CASE("the fallen pillar turns solid past frame 50 and fires its landing once") {
    test::HazardRig rig({{.kind = assets::HazardKind::FallingPillar}},
                        test::StillEntities({"piltop"}, {}));
    rig.UseSurfaces(TrackSurfaces(Surfaces()));
    rig.Hear(kFallingPillarTrigger, Start);
    rig.Hear(kFallingPillarTrigger, End);

    CHECK(CountLandings(rig, Milliseconds{1656}) == 0);
    CHECK_FALSE(rig.Surfaces().IsSolid(0));
    CHECK(CountLandings(rig, Milliseconds{3000}) == 1);

    CHECK(rig.Surfaces().IsSolid(0));
    CHECK_FALSE(rig.Surfaces().LetsShotsThrough(0));
    CHECK(rig.Entities().IsHoldingLastFrame(0));
    CHECK(rig.Hazards().Slots()[0].phase == HazardPhase::Active);
}

}
}
