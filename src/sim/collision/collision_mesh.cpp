#include "sim/collision/collision_mesh.hpp"

#include <array>
#include <cmath>
#include <format>
#include <utility>
#include <vector>

namespace rr::sim {
namespace {

struct Frame {
    std::int32_t node = 0;
    float startDistance = 0.0F;
    bool isNearVisited = false;
};

struct Point2 {
    float u = 0.0F;
    float v = 0.0F;
};

enum class Axis : std::uint8_t { X, Y, Z };

Axis LargestAxis(const Vector3& normal) {
    const float x = std::abs(normal.x);
    const float y = std::abs(normal.y);
    const float z = std::abs(normal.z);
    if (x <= y) {
        return y < z ? Axis::Z : Axis::Y;
    }
    return x < z ? Axis::Z : Axis::X;
}

Point2 Project(const Vector3& point, Axis dropped) {
    switch (dropped) {
        case Axis::X:
            return {.u = point.y, .v = point.z};
        case Axis::Y:
            return {.u = point.x, .v = point.z};
        case Axis::Z:
            return {.u = point.x, .v = point.y};
    }
    return {};
}

float EdgeValue(const Point2& from, const Point2& to, const Point2& point) {
    return ((to.v - from.v) * point.u) + ((from.u - to.u) * point.v);
}

bool IsInside(const Point2& p, const Point2& a, const Point2& b, const Point2& c) {
    const float caLimit = EdgeValue(a, c, c);
    const float caPoint = EdgeValue(a, c, p);
    const float abLimit = EdgeValue(b, a, a);
    const float abPoint = EdgeValue(b, a, p);
    const float bcLimit = EdgeValue(c, b, b);
    const float bcPoint = EdgeValue(c, b, p);
    if ((c.u - b.u) * (a.v - b.v) <= (b.v - c.v) * (b.u - a.u)) {
        return caPoint <= caLimit && abPoint <= abLimit && bcPoint <= bcLimit;
    }
    return caPoint >= caLimit && abPoint >= abLimit && bcPoint >= bcLimit;
}

Result<void> CheckTree(const assets::CollisionMeshAsset& asset) {
    std::vector<bool> hasParent(asset.nodes.size());
    for (const assets::CollisionNode& node : asset.nodes) {
        for (const std::int32_t child : {node.front, node.back}) {
            if (child == assets::kNoChild) {
                continue;
            }
            const auto index = static_cast<std::size_t>(child);
            if (child == 0 || hasParent[index]) {
                return Fail(ErrorCode::InvalidData,
                            std::format("collision node {} is reached twice", child));
            }
            hasParent[index] = true;
        }
    }
    std::vector<std::pair<std::int32_t, std::size_t>> pending{{0, 1}};
    while (!pending.empty()) {
        const auto [index, depth] = pending.back();
        pending.pop_back();
        if (depth > kMaxCollisionTreeDepth) {
            return Fail(ErrorCode::InvalidData,
                        std::format("collision tree is deeper than {}", kMaxCollisionTreeDepth));
        }
        const assets::CollisionNode& node = asset.nodes.at(static_cast<std::size_t>(index));
        for (const std::int32_t child : {node.front, node.back}) {
            if (child != assets::kNoChild) {
                pending.emplace_back(child, depth + 1);
            }
        }
    }
    return {};
}

}

Result<CollisionMesh> CollisionMesh::Create(assets::CollisionMeshAsset asset) {
    if (asset.nodes.empty()) {
        return Fail(ErrorCode::InvalidData, "collision mesh has no nodes");
    }
    return CheckTree(asset).transform([&] { return CollisionMesh(std::move(asset)); });
}

std::optional<MeshHit> CollisionMesh::CastSegment(const Vector3& from, const Vector3& to) const {
    std::array<Frame, kMaxCollisionTreeDepth> stack{};
    std::size_t depth = 0;
    const auto push = [&](std::int32_t node) {
        if (node != assets::kNoChild) {
            const assets::CollisionNode& child = asset_.nodes.at(static_cast<std::size_t>(node));
            stack.at(depth++) = {.node = node,
                                 .startDistance = Dot(child.normal, from - PlanePoint(child))};
        }
    };
    push(0);
    while (depth > 0) {
        Frame& frame = stack.at(depth - 1);
        const assets::CollisionNode& node = asset_.nodes.at(static_cast<std::size_t>(frame.node));
        const float start = frame.startDistance;
        if (!frame.isNearVisited) {
            frame.isNearVisited = true;
            push(start <= 0.0F ? node.back : node.front);
            continue;
        }
        --depth;
        const float end = Dot(node.normal, to - PlanePoint(node));
        if ((start <= 0.0F) == (end <= 0.0F)) {
            continue;
        }
        const float fraction = std::abs(start) / (std::abs(start) + std::abs(end));
        const Vector3 point = from + ((to - from) * fraction);
        if (std::optional<MeshHit> hit = TestTriangles(node, point, to - from)) {
            hit->fraction = fraction;
            return hit;
        }
        push(end <= 0.0F ? node.back : node.front);
    }
    return std::nullopt;
}

const Vector3& CollisionMesh::PlanePoint(const assets::CollisionNode& node) const {
    const assets::CollisionTriangle& first = asset_.triangles.at(node.firstTriangle);
    return asset_.vertices.at(first.vertices[0]);
}

std::optional<MeshHit> CollisionMesh::TestTriangles(const assets::CollisionNode& node,
                                                    const Vector3& point,
                                                    const Vector3& direction) const {
    const Axis dropped = LargestAxis(node.normal);
    const Point2 projected = Project(point, dropped);
    std::optional<MeshHit> backFacing;
    const std::size_t end = std::size_t{node.firstTriangle} + node.triangleCount;
    for (std::size_t i = node.firstTriangle; i < end; ++i) {
        const assets::CollisionTriangle& triangle = asset_.triangles.at(i);
        const Vector3& v0 = asset_.vertices.at(triangle.vertices[0]);
        const Vector3& v1 = asset_.vertices.at(triangle.vertices[1]);
        const Vector3& v2 = asset_.vertices.at(triangle.vertices[2]);
        if (!IsInside(projected, Project(v0, dropped), Project(v1, dropped),
                      Project(v2, dropped))) {
            continue;
        }
        const Vector3 normal = Cross(v2 - v1, v0 - v1);
        const MeshHit hit{.point = point,
                          .normal = Normalized(normal, node.normal),
                          .material = triangle.material};
        if (Dot(normal, direction) <= 0.0F) {
            return hit;
        }
        backFacing = hit;
    }
    return backFacing;
}

}
