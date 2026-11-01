#include "sim/hazard/lava_blob.hpp"

#include <doctest/doctest.h>

#include "hazard_rig.hpp"

namespace rr::sim {
namespace {

constexpr Vector3 kBlob{.x = 30.0F, .z = 2.0F};
constexpr Vector3 kNearBlob{.x = 33.0F, .z = 0.2F};

assets::Hazard LavaHazard() {
    return {.kind = assets::HazardKind::LavaBlob};
}

TrackEntitySources BlobEntity() {
    TrackEntitySources sources = test::StillEntities({"mmlavbl"}, kBlob);
    sources.entities[0].radius = 2.0F;
    return sources;
}

void TickFor(test::HazardRig& rig, int ticks) {
    for (int tick = 0; tick < ticks; ++tick) {
        rig.Tick();
    }
}

}

TEST_CASE("the lava blob splashes just after each window opens and closes, then waits 400 ms") {
    test::HazardRig rig({LavaHazard()}, BlobEntity());
    rig.Hear(kLavaBlobTrigger, EventEdge::Start);

    rig.Seek(0, 62.0F);
    rig.Tick();
    REQUIRE(rig.CountStarts(kLavaBlobEvent) == 1);
    CHECK(rig.Events()[0].position == Vector3{.x = 605.0F, .y = -500.0F, .z = 19.0F});

    rig.Seek(0, 87.0F);
    rig.Tick();
    CHECK(rig.CountStarts(kLavaBlobEvent) == 0);
    TickFor(rig, 400 / 12);
    CHECK(rig.CountStarts(kLavaBlobEvent) == 0);
    rig.Tick();
    CHECK(rig.CountStarts(kLavaBlobEvent) == 1);
}

TEST_CASE("the lava blob stays quiet between windows") {
    test::HazardRig rig({LavaHazard()}, BlobEntity());
    rig.Hear(kLavaBlobTrigger, EventEdge::Start);

    rig.Seek(0, 40.0F);
    rig.Tick();

    CHECK(rig.Events().empty());
}

TEST_CASE("the lava blob spins moving racers, shielded or not") {
    test::HazardRig rig({LavaHazard()}, BlobEntity(), 2);
    rig.Hear(kLavaBlobTrigger, EventEdge::Start);
    rig.Place(0, kNearBlob);
    rig.Place(1, kNearBlob);
    rig.Drive(5);
    rig.Car(0).RaiseShield(1);

    rig.Tick();
    rig.Touch();

    CHECK(rig.Car(0).IsSpinning());
    CHECK(rig.Car(1).IsSpinning());
    CHECK(rig.Car(1).Status().hitReactions == 1);
}

TEST_CASE("the lava blob leaves stopped racers alone") {
    test::HazardRig rig({LavaHazard()}, BlobEntity());
    rig.Hear(kLavaBlobTrigger, EventEdge::Start);
    rig.Place(0, kNearBlob);

    rig.Tick();
    rig.Touch();

    CHECK_FALSE(rig.Car(0).IsSpinning());
}

}
