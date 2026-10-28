#pragma once

#include <optional>
#include <span>
#include <vector>

#include "assets/surface_table.hpp"
#include "core/vector3.hpp"
#include "sim/collision/collision_mesh.hpp"
#include "sim/physics/body_state.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/surface.hpp"

namespace rr::sim {

struct MaterialBinding {
    Surface surface;
    bool isSolid = true;
};

struct Placement {
    Vector3 position;
    Basis basis;
};

/// Checkpoint gates are names starting with a digit, and only in the checkpoint world.
[[nodiscard]] std::vector<MaterialBinding> BindMaterials(
    const assets::CollisionMeshAsset& mesh, std::span<const assets::SurfaceEntry> surfaces,
    bool isCheckpointWorld);

class CollisionWorld {
public:
    /// Queried in world space, ignoring any placement.
    [[nodiscard]] static CollisionWorld Track(CollisionMesh mesh,
                                              std::vector<MaterialBinding> materials);

    [[nodiscard]] static CollisionWorld Placed(CollisionMesh mesh,
                                               std::vector<MaterialBinding> materials,
                                               const Placement& placement);

    /// The point and normal come back in world space.
    [[nodiscard]] std::optional<SegmentHit> CastSegment(const Vector3& from,
                                                        const Vector3& to) const;

    [[nodiscard]] const CollisionMesh& Mesh() const { return mesh_; }
    [[nodiscard]] std::span<const MaterialBinding> Materials() const { return materials_; }
    [[nodiscard]] const std::optional<Placement>& WorldPlacement() const { return placement_; }

private:
    CollisionWorld(CollisionMesh mesh, std::vector<MaterialBinding> materials,
                   std::optional<Placement> placement);

    CollisionMesh mesh_;
    std::vector<MaterialBinding> materials_;
    std::optional<Placement> placement_;
};

}
