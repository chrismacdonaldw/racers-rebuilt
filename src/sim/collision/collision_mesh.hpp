#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "assets/collision_mesh.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::sim {

inline constexpr std::size_t kMaxCollisionTreeDepth = 64;

struct MeshHit {
    Vector3 point;
    Vector3 normal;
    float fraction = 0.0F;
    std::uint16_t material = 0;
};

class CollisionMesh {
public:
    /// Fails unless the nodes form one tree from node 0, at most `kMaxCollisionTreeDepth` deep.
    [[nodiscard]] static Result<CollisionMesh> Create(assets::CollisionMeshAsset asset);

    /// Back faces count. Allocates nothing.
    [[nodiscard]] std::optional<MeshHit> CastSegment(const Vector3& from, const Vector3& to) const;

    [[nodiscard]] const assets::CollisionMeshAsset& Asset() const { return asset_; }

private:
    explicit CollisionMesh(assets::CollisionMeshAsset asset) : asset_(std::move(asset)) {}

    [[nodiscard]] const Vector3& PlanePoint(const assets::CollisionNode& node) const;
    [[nodiscard]] std::optional<MeshHit> TestTriangles(const assets::CollisionNode& node,
                                                       const Vector3& point,
                                                       const Vector3& direction) const;

    assets::CollisionMeshAsset asset_;
};

}
