#include "app/collision_wireframe.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "app/track_space.hpp"
#include "assets/archive_survey.hpp"
#include "assets/track_loader.hpp"
#include "render/renderer.hpp"
#include "sim/collision/test_meshes.hpp"

namespace rr::app {
namespace {

sim::TrackCollision OneWorld(const std::vector<sim::MaterialBinding>& materials,
                             const std::optional<sim::Placement>& placement = std::nullopt) {
    sim::CollisionMesh mesh = sim::CollisionMesh::Create(test::FloorAndWall()).value();
    sim::TrackCollision collision;
    collision.worlds.push_back(
        {.name = "collide",
         .world = placement ? sim::CollisionWorld::Placed(std::move(mesh), materials, *placement)
                            : sim::CollisionWorld::Track(std::move(mesh), materials)});
    return collision;
}

TEST_CASE("the wireframe draws each shared edge once, coloured by solidity") {
    const std::vector<render::DebugLine> lines =
        BuildCollisionWireframe(OneWorld({{}, {.isSolid = false}}));

    REQUIRE(lines.size() == 10);
    const auto solid = std::ranges::count_if(
        lines, [&](const render::DebugLine& line) { return line.color.r == lines[0].color.r; });
    CHECK(solid == 5);
    const Vector3 corner = ToEngine({.x = -10.0F, .y = -10.0F});
    CHECK(std::ranges::any_of(lines, [&](const render::DebugLine& line) {
        return line.from == corner || line.to == corner;
    }));
}

TEST_CASE("placed worlds are drawn where they are placed") {
    const std::vector<render::DebugLine> lines =
        BuildCollisionWireframe(OneWorld({}, sim::Placement{.position = {.z = 100.0F}}));

    CHECK(std::ranges::all_of(lines, [](const render::DebugLine& line) {
        return line.from.y >= 100.0F && line.to.y >= 100.0F;
    }));
}

std::size_t MostWireframeLines(const assets::Vfs& vfs) {
    std::size_t mostLines = 0;
    for (const std::string& folder : assets::ListRaceTracks(vfs)) {
        const Result<sim::TrackCollision> collision = sim::LoadTrackCollision(vfs, folder);
        REQUIRE_MESSAGE(collision.has_value(), folder);
        mostLines = std::max(mostLines, BuildCollisionWireframe(collision.value()).size());
    }
    return mostLines;
}

TEST_CASE("every retail track's wireframe fits in the debug line limit") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const std::size_t mostLines = MostWireframeLines(*vfs);

    MESSAGE("most wireframe lines: ", mostLines);
    CHECK(mostLines <= render::kMaxDebugLines);
}

}
}
