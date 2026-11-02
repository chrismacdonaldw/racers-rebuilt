#include "sim/hazard/curse_drop.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>

#include "core/mirror.hpp"
#include "hazard_rig.hpp"

namespace rr::sim {
namespace {

constexpr std::array<Vector3, 3> kDropPoints{{{.x = -357.583F, .y = 458.127F, .z = -11.665F},
                                              {.x = -351.203F, .y = 518.713F, .z = -94.216F},
                                              {.x = -362.258F, .y = 397.304F, .z = -94.960F}}};

}

TEST_CASE("the curse drop casts an ownerless level 3 curse at one of its points") {
    test::HazardRig rig({{.kind = assets::HazardKind::CurseDrop}}, {});

    rig.Hear(kCurseDropTrigger, EventEdge::Start);

    REQUIRE(rig.Powerups().Active().size() == 1);
    CHECK(rig.Effect().kind == PowerupKind::Curse);
    CHECK(rig.Effect().use.level == 3);
    CHECK(rig.Effect().use.hazard == 0);
    CHECK(std::ranges::contains(kDropPoints, rig.Hazards().Aims()[0].target));
}

TEST_CASE("the curse drop casts once per timer start") {
    test::HazardRig rig({{.kind = assets::HazardKind::CurseDrop}}, {});

    rig.Hear(kCurseDropTrigger, EventEdge::Start);
    rig.Hear(kCurseDropTrigger, EventEdge::Start);
    rig.Hear(kCurseDropTrigger, EventEdge::End);
    rig.Hear(kCurseDropTrigger, EventEdge::Start);

    CHECK(rig.Powerups().Active().size() == 2);
}

TEST_CASE("on a mirrored track the curse drop casts at a mirrored point, still aiming +X") {
    test::HazardRig rig({{.kind = assets::HazardKind::CurseDrop}}, {});
    rig.MirrorTrack();

    rig.Hear(kCurseDropTrigger, EventEdge::Start);

    CHECK(std::ranges::contains(kDropPoints, MirroredY(rig.Hazards().Aims()[0].target)));
    CHECK(rig.Hazards().Aims()[0].direction == Vector3{.x = 1.0F});
}

}
