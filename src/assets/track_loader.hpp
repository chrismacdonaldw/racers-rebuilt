#pragma once

#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/asset_cache.hpp"
#include "assets/geometry_asset.hpp"
#include "assets/image_asset.hpp"
#include "assets/loader_pool.hpp"
#include "assets/material_library.hpp"
#include "assets/model_files.hpp"
#include "assets/texture_database.hpp"
#include "assets/vfs.hpp"
#include "assets/world_database.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// `GAMEDATA` folders with a start grid and checkpoints, such as `RACEC0R0`, upper case, sorted.
[[nodiscard]] std::vector<std::string> ListRaceTracks(const Vfs& vfs);

/// What a track's emitter and ambient files reuse: `C0R0` for `RACEC0R0`, the whole name otherwise.
[[nodiscard]] std::string_view RaceTrackStem(std::string_view track);

template <typename T>
using NameMap = std::map<std::string, T, std::less<>>;

/// Maps are keyed by uppercase name, as the worlds, models and materials refer to them. The sky
/// world, `BACKGRD.WDB`, is kept out of `worlds`, and is null when not loaded.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct TrackAssets {
    std::vector<AssetCache<WorldAsset>::Handle> worlds;
    AssetCache<WorldAsset>::Handle skyWorld;
    NameMap<AssetCache<GeometryAsset>::Handle> models;
    NameMap<MaterialAsset> materials;
    NameMap<AssetCache<ImageAsset>::Handle> textures;
    std::vector<std::string> missingTextures;
};

/// Keeps `into`'s own entry where both have a name.
void MergeTrackAssets(TrackAssets& into, TrackAssets from);

class NameResolver;

/// Main thread only. `vfs` and `pool` must outlive the loader.
class TrackLoader {
public:
    TrackLoader(const Vfs& vfs, LoaderPool& pool) : vfs_(&vfs), pool_(&pool) {}

    /// Loads every WDB in `folder` and the files they name, looked up in `folder`, then in
    /// `GAMEDATA/COMMON`. A mirrored load reflects all but the sky world and its models.
    [[nodiscard]] Result<TrackAssets> Load(std::string_view folder, bool isMirrored = false);

    /// Loads only the named worlds, in their order, then the files they name, as `Load` does.
    [[nodiscard]] Result<TrackAssets> LoadWorlds(std::string_view folder,
                                                 std::span<const std::string> worlds);

    /// Looks names up as `Load` does, without any worlds, skipping empty ones.
    [[nodiscard]] Result<TrackAssets> LoadModels(std::string_view folder,
                                                 std::span<const ModelFiles> models,
                                                 bool isMirrored = false);

    /// Drops cached files that no loaded track still uses.
    void Prune();

private:
    struct Names;

    [[nodiscard]] Result<TrackAssets> LoadWorldFiles(const NameResolver& resolver,
                                                     std::span<const std::string> paths,
                                                     bool isMirrored);
    [[nodiscard]] Result<TrackAssets> LoadNamed(const NameResolver& resolver,
                                                std::vector<AssetCache<WorldAsset>::Handle> worlds,
                                                const Names& names, bool isMirrored);

    const Vfs* vfs_;
    LoaderPool* pool_;
    AssetCache<WorldAsset> worlds_;
    AssetCache<GeometryAsset> models_;
    AssetCache<std::vector<MaterialAsset>> materialLibraries_;
    AssetCache<std::vector<TextureDefinition>> textureDatabases_;
    AssetCache<ImageAsset> images_;
};

}
