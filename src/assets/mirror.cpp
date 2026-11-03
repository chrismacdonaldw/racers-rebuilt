#include "assets/mirror.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "assets/game_path.hpp"
#include "core/mirror.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kSkyWorld = "BACKGRD.WDB";

void Mirror(Vector3& v) {
    v = MirroredY(v);
}

void Mirror(std::optional<Vector3>& v) {
    if (v) {
        Mirror(*v);
    }
}

void MirrorEntities(std::span<WorldEntity> entities) {
    for (WorldEntity& entity : entities) {
        Mirror(entity.position);
        Mirror(entity.forward);
        Mirror(entity.up);
    }
}

void Mirror(CannonShot& shot) {
    Mirror(shot.launch);
    Mirror(shot.target);
    Mirror(shot.triggerCentre);
}

void Mirror(std::span<AimPoint> points) {
    for (AimPoint& point : points) {
        Mirror(point.position);
    }
}

}

void MirrorY(WorldAsset& world) {
    MirrorEntities(world.modelEntities);
    MirrorEntities(world.animatedEntities);
    MirrorEntities(world.collidableEntities);
    MirrorEntities(world.boundedEntities);
    for (WorldSprite& sprite : world.sprites) {
        Mirror(sprite.position);
        Mirror(sprite.axis);
    }
}

void MirrorY(SkeletonAsset& skeleton) {
    for (SkeletonNode& node : skeleton.nodes) {
        Mirror(node.position);
        node.rotation = MirroredY(node.rotation);
    }
}

void MirrorY(AnimationAsset& animations) {
    for (Vector3& key : animations.positionKeys) {
        Mirror(key);
    }
    for (Quaternion& key : animations.rotationKeys) {
        key = MirroredY(key);
    }
    for (AnimationPart& part : animations.parts) {
        Mirror(part.velocity);
        Mirror(part.bounds.centre);
    }
}

void MirrorY(CheckpointsAsset& checkpoints) {
    for (Checkpoint& checkpoint : checkpoints.checkpoints) {
        Mirror(checkpoint.normal);
        Mirror(checkpoint.position);
    }
}

void MirrorY(StartGridAsset& grid) {
    for (StartSlot& slot : grid.slots) {
        Mirror(slot.position);
        Mirror(slot.forward);
        Mirror(slot.up);
    }
}

void MirrorY(CollisionMeshAsset& mesh) {
    for (Vector3& vertex : mesh.vertices) {
        Mirror(vertex);
    }
    for (CollisionTriangle& triangle : mesh.triangles) {
        std::swap(triangle.vertices[1], triangle.vertices[2]);
    }
    for (CollisionNode& node : mesh.nodes) {
        Mirror(node.normal);
    }
}

void MirrorY(std::vector<SurfaceEntry>& surfaces) {
    for (SurfaceEntry& surface : surfaces) {
        Mirror(surface.vector2C);
        Mirror(surface.force);
    }
}

void MirrorY(BrickLayout& bricks) {
    for (ColourBrickPlacement& brick : bricks.coloured) {
        Mirror(brick.position);
    }
    for (Vector3& brick : bricks.white) {
        Mirror(brick);
    }
}

void MirrorY(std::vector<TargetPoint>& targets) {
    for (TargetPoint& target : targets) {
        Mirror(target.position);
    }
}

void MirrorY(EventTableAsset& table) {
    for (SoundResource& sound : table.sounds) {
        Mirror(sound.position);
    }
    for (ParticleResource& particle : table.particles) {
        Mirror(particle.position);
        Mirror(particle.direction);
        Mirror(particle.up);
    }
    for (LookTargetResource& target : table.lookTargets) {
        Mirror(target.position);
    }
    for (ExternalForceResource& force : table.externalForces) {
        Mirror(force.force);
    }
}

void MirrorY(std::vector<Hazard>& hazards) {
    for (Hazard& hazard : hazards) {
        if (auto* shot = std::get_if<CannonShot>(&hazard.payload)) {
            Mirror(*shot);
        } else if (auto* ship = std::get_if<ShipCannons>(&hazard.payload)) {
            Mirror(ship->launches);
            Mirror(ship->targets);
            Mirror(ship->shot);
        } else if (auto* lightning = std::get_if<LightningCannon>(&hazard.payload)) {
            Mirror(lightning->source);
        }
    }
}

void MirrorY(GeometryAsset& geometry) {
    for (GeometryVertex& vertex : geometry.vertices) {
        Mirror(vertex.position);
        Mirror(vertex.normal);
    }
    for (std::size_t corner = 0; corner + 2 < geometry.indices.size(); corner += 3) {
        std::swap(geometry.indices[corner + 1], geometry.indices[corner + 2]);
    }
}

bool IsMirroredWorld(std::string_view path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::string_view file = slash == std::string_view::npos ? path : path.substr(slash + 1);
    return ToUpperAscii(file) != kSkyWorld;
}

}
