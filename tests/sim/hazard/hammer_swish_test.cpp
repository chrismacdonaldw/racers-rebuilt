#include "sim/hazard/hammer_swish.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <span>

#include "hazard_rig.hpp"
#include "sim/hazard/hazards.hpp"

namespace rr::sim {
namespace {

using enum EventEdge;

TEST_CASE("the hammer swishes once in each strike window, then waits to re-arm") {
    test::HazardRig rig({{.kind = assets::HazardKind::HammerSwish}},
                        test::StillEntities({"rkhamm02"}, {}));
    rig.Hear(kHammerSwishTrigger, Start);

    rig.Seek(0, 25.0F);
    rig.Tick();
    CHECK(rig.CountStarts(kHammerSwishEvent) == 1);
    rig.Tick();
    CHECK(rig.CountStarts(kHammerSwishEvent) == 0);
    rig.Seek(0, 75.0F);
    rig.Tick();
    CHECK(rig.CountStarts(kHammerSwishEvent) == 0);
    rig.Seek(0, 50.0F);
    rig.Tick();
    rig.Seek(0, 75.0F);
    rig.Tick();
    CHECK(rig.CountStarts(kHammerSwishEvent) == 1);
}

TEST_CASE("a stopped hammer doesn't swish") {
    test::HazardRig rig({{.kind = assets::HazardKind::HammerSwish}},
                        test::StillEntities({"rkhamm02"}, {}));
    rig.Seek(0, 25.0F);

    rig.Tick();

    CHECK(rig.Events().empty());
}

TEST_CASE("the retail hammer swishes twice a swing") {
    const std::optional<RaceTrack> track = test::RetailRaceTrack("RACEC0R0");
    if (!track) {
        return;
    }
    TrackEntities entities(track->entities);
    Random random(1);
    Hazards hazards(track->hazards, entities, random);
    TrackEvents events({}, 1, false, random, Hazards::MostEventsPerTick(track->hazards, 1));
    HazardContext context{.entities = &entities, .events = &events};
    events.Fire({.id = kHammerSwishTrigger});
    hazards.Hear(events, context);

    std::size_t swishes = 0;
    for (int tick = 0; tick < 3367 / 12; ++tick) {
        entities.Tick(Milliseconds{12});
        events.BeginTick();
        hazards.Tick(context);
        for (const TrackEvent& event : events.Events()) {
            swishes += event.id == kHammerSwishEvent && event.edge == Start ? 1 : 0;
        }
    }

    CHECK(swishes == 2);
}

}
}
