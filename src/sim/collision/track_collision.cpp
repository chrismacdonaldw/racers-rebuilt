#include "sim/collision/track_collision.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <utility>

#include "assets/collision_mesh.hpp"
#include "assets/game_path.hpp"
#include "assets/surface_table.hpp"
#include "assets/world_database.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr std::array<std::string_view, 5> kTrackWorldNames{"collide", "igcollid", "cm", "titcoll",
                                                           "aracol"};
constexpr std::string_view kCheckpointPrefix = "chckpt";

bool IsTrackWorld(std::string_view name) {
    return std::ranges::contains(kTrackWorldNames, name);
}

bool IsCheckpointWorld(std::string_view name) {
    return name.starts_with(kCheckpointPrefix);
}

auto PrefixPath(std::string_view path) {
    return [path](Error error) {
        error.message = std::format("{}: {}", path, error.message);
        return error;
    };
}

template <typename Parse>
auto ReadParsed(const assets::Vfs& vfs, const std::string& path, Parse parse) {
    return vfs.Read(path)
        .and_then([&](const assets::FileData& data) { return parse(data.Bytes()); })
        .transform_error(PrefixPath(path));
}

CollisionWorld MakeWorld(CollisionMesh mesh, std::vector<MaterialBinding> materials,
                         const assets::WorldEntity& entity) {
    if (IsTrackWorld(entity.name)) {
        return CollisionWorld::Track(std::move(mesh), std::move(materials));
    }
    const Placement placement{.position = entity.position,
                              .basis = OrthonormalBasis(entity.forward, entity.up)};
    return CollisionWorld::Placed(std::move(mesh), std::move(materials), placement);
}

Result<NamedWorld> LoadWorld(const assets::Vfs& vfs, std::string_view folder,
                             const std::string& volume, const assets::WorldEntity& entity,
                             std::span<const assets::SurfaceEntry> surfaces) {
    const std::string path =
        std::format("GAMEDATA/{}/{}.BVB", folder, assets::ToUpperAscii(volume));
    return ReadParsed(vfs, path, assets::ParseCollisionMesh)
        .and_then([&](assets::CollisionMeshAsset asset) {
            std::vector<MaterialBinding> materials =
                BindMaterials(asset, surfaces, IsCheckpointWorld(entity.name));
            return CollisionMesh::Create(std::move(asset))
                .transform([&](CollisionMesh mesh) {
                    return NamedWorld{
                        .name = entity.name,
                        .world = MakeWorld(std::move(mesh), std::move(materials), entity)};
                })
                .transform_error(PrefixPath(path));
        });
}

Result<TrackCollision> LoadWorlds(const assets::Vfs& vfs, std::string_view folder,
                                  const assets::WorldAsset& wdb,
                                  std::span<const assets::SurfaceEntry> surfaces) {
    TrackCollision collision;
    std::optional<std::size_t> track;
    for (const assets::WorldEntity& entity : wdb.boundedEntities) {
        if (entity.boundingVolume == assets::kNoIndex) {
            continue;
        }
        const std::string& volume =
            wdb.boundingVolumes.at(static_cast<std::size_t>(entity.boundingVolume));
        Result<NamedWorld> world = LoadWorld(vfs, folder, volume, entity, surfaces);
        if (!world) {
            return std::unexpected(std::move(world.error()));
        }
        if (IsTrackWorld(entity.name)) {
            track = collision.worlds.size();
        } else if (IsCheckpointWorld(entity.name)) {
            collision.checkpoints = collision.worlds.size();
        }
        collision.worlds.push_back(std::move(*world));
    }
    if (!track) {
        return Fail(ErrorCode::InvalidData, std::format("{} has no track collision world", folder));
    }
    collision.track = *track;
    return collision;
}

}

Result<TrackCollision> LoadTrackCollision(const assets::Vfs& vfs, std::string_view folder) {
    return ReadParsed(vfs, std::format("GAMEDATA/{}/{}.TMB", folder, folder),
                      assets::ParseSurfaceTable)
        .and_then([&](std::vector<assets::SurfaceEntry> surfaces) {
            return ReadParsed(vfs, std::format("GAMEDATA/{}/COLLIDE.WDB", folder),
                              assets::ParseWorld)
                .and_then([&](const assets::WorldAsset& wdb) {
                    return LoadWorlds(vfs, folder, wdb, surfaces);
                })
                .transform([&](TrackCollision collision) {
                    collision.surfaces = std::move(surfaces);
                    return collision;
                });
        });
}

}
