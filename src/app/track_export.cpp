#include "app/track_export.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "app/gltf_models.hpp"
#include "app/model_export.hpp"
#include "app/parsed_file.hpp"
#include "app/race_scene_name.hpp"
#include "app/track_info.hpp"
#include "app/track_marker_nodes.hpp"
#include "assets/collision_mesh.hpp"
#include "assets/game_path.hpp"
#include "assets/geometry_database.hpp"
#include "assets/gltf_mesh.hpp"
#include "assets/gltf_scene.hpp"
#include "assets/skeleton_database.hpp"
#include "assets/track_loader.hpp"
#include "assets/world_database.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "sim/collision/track_collision.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::app {
namespace {

using assets::GltfNode;
using assets::GltfScene;

constexpr std::string_view kCommonFolder = "GAMEDATA/COMMON";
constexpr std::string_view kCollisionWorld = "COLLIDE.WDB";
constexpr std::string_view kWorldExtension = ".WDB";

Quaternion Facing(const assets::WorldEntity& entity) {
    return sim::ToQuaternion(sim::OrthonormalBasis(entity.forward, entity.up));
}

assets::GeometryAsset CollisionGeometry(const assets::CollisionMeshAsset& mesh) {
    assets::GeometryAsset geometry{.materials = mesh.materials};
    geometry.vertices.reserve(mesh.vertices.size());
    for (const Vector3& vertex : mesh.vertices) {
        geometry.vertices.push_back({.position = vertex});
    }
    for (std::uint32_t material = 0; material < mesh.materials.size(); ++material) {
        const auto first = static_cast<std::uint32_t>(geometry.indices.size());
        for (const assets::CollisionTriangle& triangle : mesh.triangles) {
            if (triangle.material == material) {
                geometry.indices.insert(geometry.indices.end(), triangle.vertices.begin(),
                                        triangle.vertices.end());
            }
        }
        geometry.ranges.push_back(
            {.firstIndex = first,
             .indexCount = static_cast<std::uint32_t>(geometry.indices.size()) - first,
             .material = material});
    }
    return geometry;
}

std::string TrackToml(const assets::Vfs& vfs, std::string_view track) {
    const std::optional<TrackInfo> info = FindTrackInfo(vfs, track);
    const std::string name = info && !info->name.empty() ? info->name : std::string(track);
    std::string toml =
        std::format("name = {}\nlaps = {}\n", assets::JsonString(name),
                    info ? info->lapCount.value_or(kDefaultLapCount) : kDefaultLapCount);
    if (info && info->grid) {
        toml += std::format("grid = {}\n", assets::JsonString(*info->grid));
    }
    return toml;
}

class TrackScene {
public:
    TrackScene(const assets::Vfs& vfs, std::string_view track)
        : vfs_(&vfs), folder_(std::format("GAMEDATA/{}", track)) {}

    Result<void> AddWorlds() {
        Result<std::vector<assets::VfsEntry>> entries = vfs_->List(folder_);
        if (!entries) {
            return std::unexpected(std::move(entries.error()));
        }
        std::vector<std::string> names;
        for (const assets::VfsEntry& entry : *entries) {
            const std::string name = assets::ToUpperAscii(entry.name);
            if (name.ends_with(kWorldExtension) && name != kCollisionWorld) {
                names.push_back(name);
            }
        }
        std::ranges::sort(names);
        for (const std::string& name : names) {
            Result<assets::WorldAsset> world =
                ReadParsed(*vfs_, assets::JoinGamePath(folder_, name), assets::ParseWorld);
            if (!world) {
                return std::unexpected(std::move(world.error()));
            }
            AddWorld(name.substr(0, name.size() - kWorldExtension.size()), *world);
        }
        return {};
    }

    Result<void> AddCollision() {
        Result<assets::WorldAsset> world =
            ReadParsed(*vfs_, assets::JoinGamePath(folder_, kCollisionWorld), assets::ParseWorld);
        if (!world) {
            return std::unexpected(std::move(world.error()));
        }
        const std::size_t group = scene_.AddNode({.name = "collision"});
        roots_.push_back(group);
        for (const assets::WorldEntity& entity : world->boundedEntities) {
            if (entity.boundingVolume == assets::kNoIndex) {
                continue;
            }
            const std::string path = std::format(
                "{}/{}.BVB", folder_,
                assets::ToUpperAscii(
                    world->boundingVolumes.at(static_cast<std::size_t>(entity.boundingVolume))));
            Result<assets::CollisionMeshAsset> mesh =
                ReadParsed(*vfs_, path, assets::ParseCollisionMesh);
            if (!mesh) {
                return std::unexpected(std::move(mesh.error()));
            }
            GltfNode node{.name = entity.name, .mesh = scene_.AddMesh(CollisionGeometry(*mesh))};
            if (!sim::IsTrackCollisionWorld(entity.name)) {
                node.position = entity.position;
                node.rotation = Facing(entity);
            }
            const std::size_t index = scene_.AddNode(std::move(node));
            scene_.Node(group).children.push_back(index);
        }
        return {};
    }

    Result<void> AddMarkers(std::string_view track) {
        return AddTrackMarkers(*vfs_, track, scene_).transform([&](std::vector<std::size_t> roots) {
            roots_.insert(roots_.end(), roots.begin(), roots.end());
        });
    }

    [[nodiscard]] std::vector<std::byte> Build() const { return scene_.Build(roots_); }

private:
    void AddWorld(const std::string& name, const assets::WorldAsset& world) {
        const std::size_t group = scene_.AddNode({.name = name});
        roots_.push_back(group);
        for (const std::vector<assets::WorldEntity>* entities :
             {&world.modelEntities, &world.animatedEntities, &world.collidableEntities}) {
            for (const assets::WorldEntity& entity : *entities) {
                const std::optional<std::size_t> mesh = MeshOf(world, entity);
                if (!mesh) {
                    continue;
                }
                const std::size_t index = scene_.AddNode({.name = entity.name,
                                                          .position = entity.position,
                                                          .rotation = Facing(entity),
                                                          .mesh = mesh});
                scene_.Node(group).children.push_back(index);
            }
        }
    }

    std::optional<std::size_t> MeshOf(const assets::WorldAsset& world,
                                      const assets::WorldEntity& entity) {
        if (entity.lods.empty() || entity.lods.front().model == assets::kNoIndex) {
            return std::nullopt;
        }
        const std::string name = assets::ToUpperAscii(
            world.models.at(static_cast<std::size_t>(entity.lods.front().model)));
        auto found = meshes_.find(name);
        if (found == meshes_.end()) {
            found = meshes_.emplace(name, LoadMesh(name)).first;
        }
        return found->second;
    }

    std::optional<std::size_t> LoadMesh(const std::string& name) {
        const std::string file = name + ".GDB";
        const std::string local = assets::JoinGamePath(folder_, file);
        const std::string path =
            vfs_->Contains(local) ? local : assets::JoinGamePath(kCommonFolder, file);
        const Result<assets::GeometryAsset> geometry =
            ReadParsed(*vfs_, path, assets::ParseGeometry);
        if (!geometry || geometry->vertices.empty()) {
            log::Warn("Exporting the track without {}: {}", name,
                      geometry ? "it has no vertices" : geometry.error().message);
            return std::nullopt;
        }
        return scene_.AddMesh(*geometry, BindsOf(path, *geometry));
    }

    [[nodiscard]] std::vector<glm::mat4> BindsOf(const std::string& model,
                                                 const assets::GeometryAsset& geometry) const {
        for (const std::string& path : SkeletonPaths(model)) {
            if (vfs_->Contains(path)) {
                return ReadParsed(*vfs_, path, assets::ParseSkeleton)
                    .transform([&](const assets::SkeletonAsset& skeleton) {
                        return assets::BindPose(skeleton, geometry.scale);
                    })
                    .value_or(std::vector<glm::mat4>{});
            }
        }
        return {};
    }

    const assets::Vfs* vfs_;
    std::string folder_;
    GltfScene scene_;
    std::vector<std::size_t> roots_;
    std::map<std::string, std::optional<std::size_t>, std::less<>> meshes_;
};

}

Result<void> ExportTrack(const assets::Vfs& vfs, std::string_view track,
                         const std::filesystem::path& folder) {
    const std::string upper = assets::ToUpperAscii(track);
    if (!std::ranges::contains(assets::ListRaceTracks(vfs), upper)) {
        return Fail(ErrorCode::NotFound, std::format("there's no race track named {}", track));
    }
    TrackScene scene(vfs, upper);
    Result<void> built =
        scene.AddWorlds().and_then([&] { return scene.AddCollision(); }).and_then([&] {
            return scene.AddMarkers(upper);
        });
    if (!built) {
        return built;
    }
    const std::string toml = TrackToml(vfs, upper);
    return WriteExportFile(folder / "track.glb", scene.Build())
        .and_then(
            [&] { return WriteExportFile(folder / "track.toml", std::as_bytes(std::span(toml))); })
        .transform([&] { log::Info("exported {} to {}", upper, Utf8FromPath(folder)); });
}

}
