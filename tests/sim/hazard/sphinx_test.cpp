#include "sim/hazard/sphinx.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "assets/surface_table.hpp"
#include "hazard_rig.hpp"

namespace rr::sim {
namespace {

using enum EventEdge;

constexpr Vector3 kSphinxAt{.x = 40.0F, .y = -20.0F, .z = 5.0F};

std::vector<assets::SurfaceEntry> Surfaces() {
    return {{.name = "sphinx"}};
}

std::size_t CountBlowUps(const test::HazardRig& rig, EventEdge edge) {
    std::size_t count = 0;
    for (const TrackEvent& event : rig.Events()) {
        if (event.id == kSphinxBlowUpEvent && event.edge == edge) {
            CHECK(event.position == kSphinxAt);
            ++count;
        }
    }
    return count;
}

TEST_CASE("a hit blows the sphinx up, opening it to racers and shots") {
    test::HazardRig rig({{.kind = assets::HazardKind::Sphinx}},
                        test::StillEntities({"blowup"}, kSphinxAt));
    rig.UseSurfaces(TrackSurfaces(Surfaces()));

    rig.Hear(kSphinxTrigger, Start);

    CHECK(CountBlowUps(rig, Start) == 1);
    CHECK_FALSE(rig.Surfaces().IsSolid(0));
    CHECK(rig.Surfaces().LetsShotsThrough(0));
}

void TickFor(test::HazardRig& rig, Milliseconds time) {
    for (Milliseconds run{0}; run < time; run += test::kRigTick) {
        rig.Tick();
    }
}

TEST_CASE("the blow-up ends after 600 ms, leaving the sphinx destroyed") {
    test::HazardRig rig({{.kind = assets::HazardKind::Sphinx}},
                        test::StillEntities({"blowup"}, kSphinxAt));
    rig.UseSurfaces(TrackSurfaces(Surfaces()));
    rig.Hear(kSphinxTrigger, Start);
    rig.Hear(kSphinxTrigger, End);

    TickFor(rig, Milliseconds{588});
    CHECK(rig.Hazards().Slots()[0].phase == HazardPhase::Active);
    rig.Tick();

    CHECK(CountBlowUps(rig, End) == 1);
    CHECK(rig.Hazards().Slots()[0].phase == HazardPhase::Loaded);
    CHECK(std::get<Sphinx>(rig.Hazards().Slots()[0].state).isDestroyed);
}

TEST_CASE("a destroyed sphinx stays open and ignores later hits") {
    test::HazardRig rig({{.kind = assets::HazardKind::Sphinx}},
                        test::StillEntities({"blowup"}, kSphinxAt));
    rig.UseSurfaces(TrackSurfaces(Surfaces()));
    rig.Hear(kSphinxTrigger, Start);
    TickFor(rig, kSphinxBlowUp);

    rig.Hear(kSphinxTrigger, Start);

    CHECK(CountBlowUps(rig, Start) == 0);
    CHECK_FALSE(rig.Surfaces().IsSolid(0));
}

TEST_CASE("the blow-up counts up from the hit until the sphinx is destroyed") {
    test::HazardRig rig({{.kind = assets::HazardKind::Sphinx}},
                        test::StillEntities({"blowup"}, kSphinxAt));
    rig.UseSurfaces(TrackSurfaces(Surfaces()));
    const auto blowingUp = [&] {
        return std::get<Sphinx>(rig.Hazards().Slots()[0].state).BlowingUp();
    };
    CHECK_FALSE(blowingUp());

    rig.Hear(kSphinxTrigger, Start);
    rig.Tick();
    CHECK(blowingUp() == test::kRigTick);
    TickFor(rig, kSphinxBlowUp - test::kRigTick);

    CHECK_FALSE(blowingUp());
}

}
}
