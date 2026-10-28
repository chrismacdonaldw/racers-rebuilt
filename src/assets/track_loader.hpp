#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "assets/asset_cache.hpp"
#include "assets/geometry_asset.hpp"
#include "assets/image_asset.hpp"
#include "assets/loader_pool.hpp"
#include "assets/material_library.hpp"
#include "assets/texture_database.hpp"
#include "assets/vfs.hpp"
#include "assets/world_database.hpp"
#include "core/result.hpp"

namespace rr::assets {

template <typename T>
using NameMap = std::map<std::string, T, std::less<>>;

/// Maps are keyed by uppercase name, as the worlds, models and materials refer to them.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct TrackAssets {
    std::vector<AssetCache<WorldAsset>::Handle> worlds;
    NameMap<AssetCache<GeometryAsset>::Handle> models;
    NameMap<MaterialAsset> materials;
    NameMap<AssetCache<ImageAsset>::Handle> textures;
};

/// Main thread only. `vfs` and `pool` must outlive the loader.
class TrackLoader {
public:
    TrackLoader(const Vfs& vfs, LoaderPool& pool) : vfs_(&vfs), pool_(&pool) {}

    /// Loads every WDB in `folder` and the files they name, looked up in `folder`, then in
    /// `GAMEDATA/COMMON`.
    [[nodiscard]] Result<TrackAssets> Load(std::string_view folder);

    /// Drops cached files that no loaded track still uses.
    void Prune();

private:
    const Vfs* vfs_;
    LoaderPool* pool_;
    AssetCache<WorldAsset> worlds_;
    AssetCache<GeometryAsset> models_;
    AssetCache<std::vector<MaterialAsset>> materialLibraries_;
    AssetCache<std::vector<TextureDefinition>> textureDatabases_;
    AssetCache<ImageAsset> images_;
};

}
