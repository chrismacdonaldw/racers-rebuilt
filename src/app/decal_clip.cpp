#include "app/decal_clip.hpp"

#include <cstdint>

namespace rr::app {
namespace {

enum class Axis : std::uint8_t { U, V };

struct Edge {
    Axis axis = Axis::U;
    float at = 0.0F;
    float stored = 0.0F;
    bool keepsAbove = true;
};

constexpr float kFarEdgeStored = 0.99F;

constexpr std::array kEdges{
    Edge{.axis = Axis::U, .at = 0.0F, .stored = 0.0F, .keepsAbove = true},
    Edge{.axis = Axis::U, .at = 1.0F, .stored = kFarEdgeStored, .keepsAbove = false},
    Edge{.axis = Axis::V, .at = 0.0F, .stored = 0.0F, .keepsAbove = true},
    Edge{.axis = Axis::V, .at = 1.0F, .stored = kFarEdgeStored, .keepsAbove = false},
};

float CoordinateOf(const DecalVertex& vertex, Axis axis) {
    return axis == Axis::U ? vertex.u : vertex.v;
}

bool IsInside(const DecalVertex& vertex, const Edge& edge) {
    const float coordinate = CoordinateOf(vertex, edge.axis);
    return edge.keepsAbove ? coordinate >= edge.at : coordinate <= edge.at;
}

DecalVertex Cut(const DecalVertex& from, const DecalVertex& to, const Edge& edge) {
    const float start = CoordinateOf(from, edge.axis);
    const float along = (edge.at - start) / (CoordinateOf(to, edge.axis) - start);
    DecalVertex cut{.position = Lerp(from.position, to.position, along),
                    .u = from.u + ((to.u - from.u) * along),
                    .v = from.v + ((to.v - from.v) * along)};
    (edge.axis == Axis::U ? cut.u : cut.v) = edge.stored;
    return cut;
}

void Push(DecalPolygon& polygon, const DecalVertex& vertex) {
    polygon.corners.at(polygon.count) = vertex;
    ++polygon.count;
}

DecalPolygon ClipEdge(const DecalPolygon& polygon, const Edge& edge) {
    DecalPolygon clipped;
    for (std::size_t corner = 0; corner < polygon.count; ++corner) {
        const DecalVertex& previous =
            polygon.corners.at((corner + polygon.count - 1) % polygon.count);
        const DecalVertex& current = polygon.corners.at(corner);
        const bool isInside = IsInside(current, edge);
        if (isInside != IsInside(previous, edge)) {
            Push(clipped, Cut(previous, current, edge));
        }
        if (isInside) {
            Push(clipped, current);
        }
    }
    return clipped;
}

}

DecalPolygon ClipToDecal(const std::array<DecalVertex, 3>& triangle) {
    DecalPolygon polygon{.corners = {triangle[0], triangle[1], triangle[2]}, .count = 3};
    for (const Edge& edge : kEdges) {
        polygon = ClipEdge(polygon, edge);
    }
    return polygon;
}

}
