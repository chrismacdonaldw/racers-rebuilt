#include "app/collision_wireframe.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "app/track_space.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::app {
namespace {

constexpr render::Color kSolidColor{.r = 0.2F, .g = 0.9F, .b = 0.3F};
constexpr render::Color kPassableColor{.r = 0.95F, .g = 0.8F, .b = 0.2F};
constexpr render::Color kPlacedColor{.r = 0.3F, .g = 0.6F, .b = 1.0F};

struct Edge {
    std::uint16_t from = 0;
    std::uint16_t to = 0;
    bool isSolid = true;
};

std::vector<Edge> UniqueEdges(const sim::CollisionWorld& world) {
    const assets::CollisionMeshAsset& mesh = world.Mesh().Asset();
    std::vector<Edge> edges;
    edges.reserve(mesh.triangles.size() * 3);
    for (const assets::CollisionTriangle& triangle : mesh.triangles) {
        const bool isSolid = world.Materials()[triangle.material].isSolid;
        for (std::size_t i = 0; i < triangle.vertices.size(); ++i) {
            const auto [low, high] =
                std::minmax(triangle.vertices.at(i), triangle.vertices.at((i + 1) % 3));
            edges.push_back({.from = low, .to = high, .isSolid = isSolid});
        }
    }
    const auto key = [](const Edge& edge) { return std::pair(edge.from, edge.to); };
    std::ranges::stable_sort(edges, {}, key);
    const auto duplicates = std::ranges::unique(edges, {}, key);
    edges.erase(duplicates.begin(), duplicates.end());
    return edges;
}

render::Color ColorFor(bool isPlaced, bool isSolid) {
    if (isPlaced) {
        return kPlacedColor;
    }
    return isSolid ? kSolidColor : kPassableColor;
}

Vector3 InWorld(const sim::CollisionWorld& world, const Vector3& point) {
    const std::optional<sim::Placement>& placement = world.WorldPlacement();
    return placement ? placement->position + sim::ToWorld(placement->basis, point) : point;
}

}

void DeclareDeveloperSettings(settings::Registry& registry) {
    registry.Declare(kCollisionWireframe);
}

std::vector<render::DebugLine> BuildCollisionWireframe(const sim::TrackCollision& collision) {
    std::vector<render::DebugLine> lines;
    for (const sim::NamedWorld& named : collision.worlds) {
        const sim::CollisionWorld& world = named.world;
        const std::vector<Vector3>& vertices = world.Mesh().Asset().vertices;
        const bool isPlaced = world.WorldPlacement().has_value();
        for (const Edge& edge : UniqueEdges(world)) {
            lines.push_back({.from = ToEngine(InWorld(world, vertices.at(edge.from))),
                             .to = ToEngine(InWorld(world, vertices.at(edge.to))),
                             .color = ColorFor(isPlaced, edge.isSolid)});
        }
    }
    return lines;
}

}
