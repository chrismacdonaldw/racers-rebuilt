#include "sim/hazard/hazard_box.hpp"

#include <doctest/doctest.h>

#include <variant>

#include "hazard_rig.hpp"

namespace rr::sim {
namespace {

constexpr assets::EventId kTrigger = 5;
constexpr Vector3 kBoxAt{.x = 30.0F, .z = 2.0F};

assets::Hazard Box(const char* entity, float startFrame = 0.0F) {
    return {.kind = assets::HazardKind::SwingingBox,
            .payload = assets::SwingingBox{.entity = entity,
                                           .trigger = kTrigger,
                                           .startFrame = startFrame,
                                           .size = {.x = 40.0F, .y = 10.0F, .z = 40.0F}}};
}

const HazardBox& BoxOf(const test::HazardRig& rig) {
    return std::get<HazardBox>(rig.Hazards().Slots()[0].state);
}

TEST_CASE("a racer driving into the box is pushed out and bounces back") {
    test::HazardRig rig({Box("dum02")}, test::StillEntities({"dum02"}, kBoxAt));
    rig.Hear(kTrigger, EventEdge::Start);
    rig.Place(0, {.x = 8.0F, .z = 0.2F});
    const float before = rig.Car().Body().motion.position.x;

    rig.Tick();
    rig.Touch();

    CHECK(rig.Car().Body().motion.position.x < before);
}

TEST_CASE("the box only blocks while its trigger is held") {
    test::HazardRig rig({Box("dum02")}, test::StillEntities({"dum02"}, kBoxAt));
    rig.Place(0, {.x = 8.0F, .z = 0.2F});
    const Vector3 before = rig.Car().Body().motion.position;

    rig.Tick();
    rig.Touch();

    CHECK(rig.Car().Body().motion.position == before);
}

TEST_CASE("ghosts pass through the box") {
    test::HazardRig rig({Box("dum02")}, test::StillEntities({"dum02"}, kBoxAt));
    rig.Hear(kTrigger, EventEdge::Start);
    rig.Place(0, {.x = 8.0F, .z = 0.2F});
    rig.Car().EnterGhostMode();
    const Vector3 before = rig.Car().Body().motion.position;

    rig.Tick();
    rig.Touch();

    CHECK(rig.Car().Body().motion.position == before);
}

TEST_CASE("the box sits on its entity's node and starts the animation at its start frame") {
    test::HazardRig rig({Box("dum02", 40.0F)}, test::StillEntities({"dum02"}, kBoxAt));
    rig.Hear(kTrigger, EventEdge::Start);

    rig.Tick();

    CHECK(BoxOf(rig).box.centre == kBoxAt);
    CHECK(BoxOf(rig).box.halfSize == Vector3{.x = 20.0F, .y = 5.0F, .z = 20.0F});
    CHECK_FALSE(BoxOf(rig).followsNode);
}

TEST_CASE("only a box named rk follows its node's turn") {
    const test::HazardRig rig({Box("rkhamm02")}, test::StillEntities({"rkhamm02"}, kBoxAt));

    CHECK(BoxOf(rig).followsNode);
}

}
}
