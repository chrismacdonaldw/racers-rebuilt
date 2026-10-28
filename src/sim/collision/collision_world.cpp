#include "sim/collision/collision_world.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

bool IsGateName(std::string_view name) {
    return !name.empty() && std::isdigit(static_cast<unsigned char>(name.front())) != 0;
}

MaterialBinding Bind(const assets::SurfaceEntry& entry) {
    const Surface defaults;
    return {
        .surface = {.rollingResistance =
                        entry.rollingResistance.value_or(defaults.rollingResistance),
                    .supportThreshold = entry.supportThreshold.value_or(defaults.supportThreshold),
                    .friction = entry.friction.value_or(defaults.friction),
                    .lateralGrip = entry.lateralGrip.value_or(defaults.lateralGrip),
                    .force = entry.force.value_or(defaults.force)},
        .isSolid = !entry.isNonSolid};
}

Vector3 ToLocal(const Basis& basis, const Vector3& v) {
    return {.x = Dot(basis.forward, v), .y = Dot(basis.side, v), .z = Dot(basis.up, v)};
}

}

std::vector<MaterialBinding> BindMaterials(const assets::CollisionMeshAsset& mesh,
                                           std::span<const assets::SurfaceEntry> surfaces,
                                           bool isCheckpointWorld) {
    std::vector<MaterialBinding> bindings;
    bindings.reserve(mesh.materials.size());
    for (const std::string& name : mesh.materials) {
        if (isCheckpointWorld && IsGateName(name)) {
            bindings.push_back({.isSolid = false});
            continue;
        }
        const auto entry = std::ranges::find(surfaces, name, &assets::SurfaceEntry::name);
        bindings.push_back(entry == surfaces.end() ? MaterialBinding{} : Bind(*entry));
    }
    return bindings;
}

CollisionWorld CollisionWorld::Track(CollisionMesh mesh, std::vector<MaterialBinding> materials) {
    return {std::move(mesh), std::move(materials), std::nullopt};
}

CollisionWorld CollisionWorld::Placed(CollisionMesh mesh, std::vector<MaterialBinding> materials,
                                      const Placement& placement) {
    return {std::move(mesh), std::move(materials), placement};
}

CollisionWorld::CollisionWorld(CollisionMesh mesh, std::vector<MaterialBinding> materials,
                               std::optional<Placement> placement)
    : mesh_(std::move(mesh)), materials_(std::move(materials)), placement_(placement) {
    materials_.resize(mesh_.Asset().materials.size());
}

std::optional<SegmentHit> CollisionWorld::CastSegment(const Vector3& from,
                                                      const Vector3& to) const {
    const std::optional<MeshHit> hit =
        placement_ ? mesh_.CastSegment(ToLocal(placement_->basis, from - placement_->position),
                                       ToLocal(placement_->basis, to - placement_->position))
                   : mesh_.CastSegment(from, to);
    if (!hit) {
        return std::nullopt;
    }
    const MaterialBinding& material = materials_.at(hit->material);
    SegmentHit result{.point = hit->point,
                      .normal = hit->normal,
                      .fraction = hit->fraction,
                      .surface = material.surface,
                      .isSolid = material.isSolid,
                      .isTrackWorld = !placement_};
    if (placement_) {
        result.point = placement_->position + ToWorld(placement_->basis, hit->point);
        result.normal = ToWorld(placement_->basis, hit->normal);
    }
    return result;
}

}
