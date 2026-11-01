#include "app/hazard/ghost_trail.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "../../sim/hazard/hazard_rig.hpp"
#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kGhost{.x = 30.0F, .z = 3.0F};

std::vector<render::MeshDraw> TrackDraws() {
    return {{.firstIndex = 0}, {.firstIndex = 6}, {.firstIndex = 12}};
}

std::vector<NamedDraws> Named() {
    return {{.entity = "rock", .span = {.first = 0, .count = 1}},
            {.entity = "gtrail", .span = {.first = 1, .count = 2}}};
}

}

TEST_CASE("the ghost trail draws three fading copies of its model while the ghost runs") {
    const sim::TrackEntitySources sources = test::StillEntities({"ghostly"}, kGhost);
    test::HazardRig rig({{.kind = assets::HazardKind::Ghost}}, sources);
    const GhostTrail trail(TrackDraws(), Named(), sources);
    std::vector<render::MeshDraw> draws;

    trail.Append(rig.Hazards(), rig.Entities().Clocks(), draws);
    CHECK(draws.empty());

    rig.Hear(sim::kGhostTrigger, sim::EventEdge::Start);
    trail.Append(rig.Hazards(), rig.Entities().Clocks(), draws);

    REQUIRE(draws.size() == 6);
    CHECK(draws[0].firstIndex == 6);
    CHECK(draws[1].firstIndex == 12);
    CHECK(draws[0].material.tint.a == doctest::Approx(126.0F / 255.0F));
    CHECK(draws[2].material.tint.a == doctest::Approx(84.0F / 255.0F));
    CHECK(draws[4].material.tint.a == doctest::Approx(42.0F / 255.0F));
    CHECK(draws[5].material.blend == render::BlendMode::Alpha);
    CHECK(draws[5].transform.position == ToEngine(kGhost));
}

}
