#include "app/track_entity_draws.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "app/track_space.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"

namespace rr::app {
namespace {

sim::TrackEntitySources Arm() {
    return {.skeletons = {{.nodes = {{.name = "pivot", .position = {.z = 1.0F}},
                                     {.name = "tip", .position = {.x = 10.0F}, .parent = 0}}}},
            .animations = {{}},
            .entities = {{.name = "arm",
                          .model = "arm",
                          .placement = {.position = {.x = 100.0F}},
                          .scale = 0.5F}}};
}

std::vector<Vector3> Positions(const render::MeshDraw& draw) {
    std::vector<Vector3> positions{draw.transform.position};
    for (const Transform& joint : draw.joints) {
        positions.push_back(joint.position);
    }
    return positions;
}

TEST_CASE("an entity's draws are placed at the entity and skinned by every node") {
    const sim::TrackEntitySources sources = Arm();
    const assets::NameMap<JointedModel> models{
        {"ARM", {.draws = std::vector<render::MeshDraw>(2), .joints = {0, 1}, .slots = {0, 0}}}};
    TrackEntityDraws draws(models, sources);

    draws.Update({});

    const std::vector<Vector3> expected{ToEngine({.x = 100.0F}), ToEngine({.z = 0.5F}),
                                        ToEngine({.x = 5.0F, .z = 0.5F})};
    REQUIRE(draws.Draws().size() == 2);
    for (const render::MeshDraw& draw : draws.Draws()) {
        CHECK(Positions(draw) == expected);
    }
}

TEST_CASE("an entity with a texture scroll offsets its own draws") {
    sim::TrackEntitySources sources = Arm();
    sources.entities[0].textureScroll = assets::TextureScroll{.vPerSecond = -0.5F};
    sources.entities.push_back({.name = "still", .model = "arm"});
    const assets::NameMap<JointedModel> models{
        {"ARM", {.draws = std::vector<render::MeshDraw>(2), .joints = {0, 1}, .slots = {0, 0}}}};
    TrackEntityDraws draws(models, sources);

    draws.Scroll(1024.0F);

    REQUIRE(draws.Draws().size() == 4);
    CHECK(draws.Draws()[0].material.uvOffset == render::UvOffset{.v = 0.5F});
    CHECK(draws.Draws()[1].material.uvOffset == render::UvOffset{.v = 0.5F});
    CHECK(draws.Draws()[2].material.uvOffset == render::UvOffset{});
    CHECK(draws.Draws()[3].material.uvOffset == render::UvOffset{});
}

}
}
