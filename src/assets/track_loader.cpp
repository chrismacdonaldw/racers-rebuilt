#include "assets/track_loader.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <iterator>
#include <optional>
#include <set>
#include <span>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/geometry_database.hpp"
#include "assets/merged_table.hpp"
#include "assets/mirror.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kCommonFolder = "GAMEDATA/COMMON";
constexpr std::string_view kMirroredKey = "|mirrored";
constexpr std::string_view kRaceFolder = "GAMEDATA";
constexpr std::string_view kStartGrid = "STARTPOS.SPB";
constexpr std::string_view kCheckpoints = "RACE.CPB";
constexpr std::string_view kRaceFolderPrefix = "RACE";

using NameSet = std::set<std::string, std::less<>>;

template <typename T>
using Handles = std::vector<typename AssetCache<T>::Handle>;

template <typename T>
using Loader = std::function<Result<T>(std::size_t)>;

template <typename T>
using Mirror = void (*)(T&);

struct Request {
    std::string key;
    std::string path;
    bool isMirrored = false;
};

template <typename T>
Result<Handles<T>> LoadAll(LoaderPool& pool, AssetCache<T>& cache,
                           std::span<const Request> requests, const Loader<T>& load) {
    Handles<T> handles(requests.size());
    std::vector<std::size_t> missing;
    for (std::size_t i = 0; i < requests.size(); ++i) {
        handles[i] = cache.Find(requests[i].key);
        if (!handles[i]) {
            missing.push_back(i);
        }
    }
    std::vector<std::optional<Result<T>>> parsed(missing.size());
    pool.ForEach(missing.size(), [&](std::size_t job) { parsed[job] = load(missing[job]); });
    for (std::size_t job = 0; job < missing.size(); ++job) {
        const Request& request = requests[missing[job]];
        Result<T>& result = *parsed[job];
        if (!result) {
            return Fail(result.error().code,
                        std::format("{}: {}", request.path, result.error().message));
        }
        handles[missing[job]] = cache.Insert(request.key, std::move(*result));
    }
    return handles;
}

Result<NameSet> ListFiles(const Vfs& vfs, std::string_view folder) {
    return vfs.List(folder).transform([](const std::vector<VfsEntry>& entries) {
        NameSet names;
        for (const VfsEntry& entry : entries) {
            if (entry.kind == EntryKind::File) {
                names.insert(entry.name);
            }
        }
        return names;
    });
}

}

// NOLINTNEXTLINE(bugprone-exception-escape)
class NameResolver {
public:
    static Result<NameResolver> Create(const Vfs& vfs, std::string_view folder) {
        Result<std::string> normalized = NormalizeGamePath(folder);
        Result<NameSet> local =
            normalized.and_then([&](const std::string& path) { return ListFiles(vfs, path); });
        if (!local) {
            return std::unexpected(std::move(local.error()));
        }
        return NameResolver(std::move(*normalized), std::move(*local),
                            ListFiles(vfs, kCommonFolder).value_or(NameSet{}));
    }

    [[nodiscard]] Result<std::string> Resolve(std::string_view fileName) const {
        const std::string upper = ToUpperAscii(fileName);
        if (local_.contains(upper)) {
            return JoinGamePath(folder_, upper);
        }
        if (common_.contains(upper)) {
            return JoinGamePath(kCommonFolder, upper);
        }
        return Fail(ErrorCode::NotFound,
                    std::format("{} isn't in {} or {}", upper, folder_, kCommonFolder));
    }

    [[nodiscard]] Result<std::vector<Request>> ResolveAll(const NameSet& names,
                                                          std::string_view extension) const {
        std::vector<Request> requests;
        for (const std::string& name : names) {
            Result<std::string> path = Resolve(name + std::string(extension));
            if (!path) {
                return std::unexpected(std::move(path.error()));
            }
            requests.push_back({.key = *path, .path = *path});
        }
        return requests;
    }

    [[nodiscard]] std::vector<Request> LocalFilesWithExtension(std::string_view extension) const {
        std::vector<Request> requests;
        for (const std::string& name : local_) {
            if (name.ends_with(extension)) {
                const std::string path = JoinGamePath(folder_, name);
                requests.push_back({.key = path, .path = path});
            }
        }
        return requests;
    }

private:
    NameResolver(std::string folder, NameSet local, NameSet common)
        : folder_(std::move(folder)), local_(std::move(local)), common_(std::move(common)) {}

    std::string folder_;
    NameSet local_;
    NameSet common_;
};

namespace {

struct Loading {
    const Vfs* vfs = nullptr;
    LoaderPool* pool = nullptr;
    const NameResolver* resolver = nullptr;
    bool isMirrored = false;
    const NameSet* unmirrored = nullptr;
};

bool MirrorsName(const Loading& loading, const std::string& name) {
    return loading.isMirrored &&
           (loading.unmirrored == nullptr || !loading.unmirrored->contains(name));
}

template <typename T>
Result<Handles<T>> LoadMirrorable(const Loading& loading, AssetCache<T>& cache,
                                  std::vector<Request> requests,
                                  Result<T> (*parse)(std::span<const std::byte>),
                                  Mirror<T> mirror) {
    for (Request& request : requests) {
        request.isMirrored = request.isMirrored && mirror != nullptr;
        if (request.isMirrored) {
            request.key += kMirroredKey;
        }
    }
    return LoadAll<T>(*loading.pool, cache, requests, [&](std::size_t index) {
        const Request& request = requests[index];
        Result<T> parsed = loading.vfs->Read(request.path).and_then([parse](const FileData& data) {
            return parse(data.Bytes());
        });
        if (parsed && request.isMirrored) {
            mirror(*parsed);
        }
        return parsed;
    });
}

template <typename T>
NameMap<typename AssetCache<T>::Handle> ByName(const NameSet& names, Handles<T> handles) {
    NameMap<typename AssetCache<T>::Handle> byName;
    auto handle = handles.begin();
    for (const std::string& name : names) {
        byName.emplace(name, std::move(*handle++));
    }
    return byName;
}

template <typename T>
Result<NameMap<typename AssetCache<T>::Handle>> LoadByName(
    const Loading& loading, AssetCache<T>& cache, const NameSet& names, std::string_view extension,
    Result<T> (*parse)(std::span<const std::byte>), Mirror<T> mirror = nullptr) {
    return loading.resolver->ResolveAll(names, extension)
        .and_then([&](std::vector<Request> requests) {
            auto name = names.begin();
            for (Request& request : requests) {
                request.isMirrored = MirrorsName(loading, *name++);
            }
            return LoadMirrorable<T>(loading, cache, std::move(requests), parse, mirror);
        })
        .transform([&](Handles<T> handles) { return ByName<T>(names, std::move(handles)); });
}

template <typename Entry>
Result<NameMap<typename AssetCache<std::vector<Entry>>::Handle>> LoadTablesByName(
    const Loading& loading, AssetCache<std::vector<Entry>>& cache, const NameSet& names,
    std::string_view extension, Result<std::vector<Entry>> (*parse)(std::span<const std::byte>)) {
    using Table = std::vector<Entry>;
    return loading.resolver->ResolveAll(names, extension)
        .and_then([&](const std::vector<Request>& requests) {
            return LoadAll<Table>(*loading.pool, cache, requests, [&](std::size_t index) {
                return ReadMergedTable(*loading.vfs, requests[index].path, parse);
            });
        })
        .transform(
            [&](Handles<Table> handles) { return ByName<Table>(names, std::move(handles)); });
}

std::string ImageKey(const std::string& path, const TextureDefinition& definition) {
    std::string key = path;
    if (definition.colorKey) {
        const ColorKey& color = *definition.colorKey;
        key += std::format("#{:02X}{:02X}{:02X}", color.red, color.green, color.blue);
    }
    if (definition.flipsVertically) {
        key += "^";
    }
    return key;
}

Result<NameMap<MaterialAsset>> LoadMaterials(const Loading& loading,
                                             AssetCache<std::vector<MaterialAsset>>& cache,
                                             const NameSet& names) {
    return LoadTablesByName<MaterialAsset>(loading, cache, names, ".MDB", ParseMaterialLibrary)
        .transform([](const auto& libraries) {
            NameMap<MaterialAsset> materials;
            for (const auto& [name, library] : libraries) {
                for (const MaterialAsset& material : *library) {
                    materials.try_emplace(ToUpperAscii(material.name), material);
                }
            }
            return materials;
        });
}

Result<NameMap<AssetCache<ImageAsset>::Handle>> LoadImages(
    const Loading& loading, AssetCache<ImageAsset>& cache,
    const NameMap<TextureDefinition>& definitions, std::vector<std::string>& missing) {
    std::vector<Request> requests;
    std::vector<const std::pair<const std::string, TextureDefinition>*> requested;
    for (const auto& entry : definitions) {
        Result<std::string> path = loading.resolver->Resolve(entry.second.FileName());
        if (!path) {
            missing.push_back(entry.second.FileName());
            continue;
        }
        requests.push_back({.key = ImageKey(*path, entry.second), .path = *path});
        requested.push_back(&entry);
    }
    Result<Handles<ImageAsset>> images =
        LoadAll<ImageAsset>(*loading.pool, cache, requests, [&](std::size_t index) {
            return loading.vfs->Read(requests[index].path).and_then([&](const FileData& data) {
                return DecodeTexture(data.Bytes(), requested[index]->second);
            });
        });
    if (!images) {
        return std::unexpected(std::move(images.error()));
    }
    NameMap<AssetCache<ImageAsset>::Handle> textures;
    for (std::size_t i = 0; i < requested.size(); ++i) {
        textures.emplace(requested[i]->first, std::move((*images)[i]));
    }
    return textures;
}

Result<NameMap<AssetCache<ImageAsset>::Handle>> LoadTextures(
    const Loading& loading, AssetCache<std::vector<TextureDefinition>>& databaseCache,
    AssetCache<ImageAsset>& imageCache, const NameSet& names, std::vector<std::string>& missing) {
    return LoadTablesByName<TextureDefinition>(loading, databaseCache, names, ".TDB",
                                               ParseTextureDatabase)
        .and_then([&](const auto& databases) {
            NameMap<TextureDefinition> definitions;
            for (const auto& [name, database] : databases) {
                for (const TextureDefinition& definition : *database) {
                    definitions.try_emplace(ToUpperAscii(definition.name), definition);
                }
            }
            return LoadImages(loading, imageCache, definitions, missing);
        });
}

void InsertUpper(NameSet& names, std::string_view name) {
    if (!name.empty()) {
        names.insert(ToUpperAscii(name));
    }
}

void InsertAllUpper(NameSet& names, std::span<const std::string> source) {
    for (const std::string& name : source) {
        InsertUpper(names, name);
    }
}

}

// NOLINTNEXTLINE(bugprone-exception-escape)
struct TrackLoader::Names {
    NameSet models;
    NameSet skyModels;
    NameSet materialLibraries;
    NameSet textureDatabases;
    NameSet materialAnimations;
};

Result<TrackAssets> TrackLoader::Load(std::string_view folder, bool isMirrored) {
    Result<NameResolver> resolver = NameResolver::Create(*vfs_, folder);
    if (!resolver) {
        return std::unexpected(std::move(resolver.error()));
    }
    std::vector<std::string> paths;
    for (Request& file : resolver->LocalFilesWithExtension(".WDB")) {
        paths.push_back(std::move(file.path));
    }
    return LoadWorldFiles(*resolver, paths, isMirrored);
}

Result<TrackAssets> TrackLoader::LoadWorlds(std::string_view folder,
                                            std::span<const std::string> worlds) {
    Result<NameResolver> resolver = NameResolver::Create(*vfs_, folder);
    if (!resolver) {
        return std::unexpected(std::move(resolver.error()));
    }
    std::vector<std::string> paths;
    for (const std::string& world : worlds) {
        Result<std::string> path = resolver->Resolve(world + ".WDB");
        if (!path) {
            return std::unexpected(std::move(path.error()));
        }
        paths.push_back(std::move(*path));
    }
    return LoadWorldFiles(*resolver, paths, false);
}

Result<TrackAssets> TrackLoader::LoadWorldFiles(const NameResolver& resolver,
                                                std::span<const std::string> paths,
                                                bool isMirrored) {
    std::vector<Request> files;
    for (const std::string& path : paths) {
        files.push_back(
            {.key = path, .path = path, .isMirrored = isMirrored && IsMirroredWorld(path)});
    }
    const Loading loading{
        .vfs = vfs_, .pool = pool_, .resolver = &resolver, .isMirrored = isMirrored};
    Result<Handles<WorldAsset>> worlds =
        LoadMirrorable<WorldAsset>(loading, worlds_, files, ParseWorld, MirrorY);
    if (!worlds) {
        return std::unexpected(std::move(worlds.error()));
    }
    Names names;
    Handles<WorldAsset> placed;
    AssetCache<WorldAsset>::Handle sky;
    for (std::size_t i = 0; i < worlds->size(); ++i) {
        AssetCache<WorldAsset>::Handle& world = (*worlds)[i];
        InsertAllUpper(names.models, world->models);
        InsertAllUpper(names.models, world->materialTables);
        InsertAllUpper(names.materialLibraries, world->materialLibraries);
        InsertAllUpper(names.textureDatabases, world->textureLists);
        InsertAllUpper(names.materialAnimations, world->materialAnimations);
        if (IsMirroredWorld(files[i].path)) {
            placed.push_back(std::move(world));
        } else {
            InsertAllUpper(names.skyModels, world->models);
            sky = std::move(world);
        }
    }
    return LoadNamed(resolver, std::move(placed), names, isMirrored)
        .transform([&](TrackAssets assets) {
            assets.skyWorld = std::move(sky);
            return assets;
        });
}

Result<TrackAssets> TrackLoader::LoadModels(std::string_view folder,
                                            std::span<const ModelFiles> models, bool isMirrored) {
    Result<NameResolver> resolver = NameResolver::Create(*vfs_, folder);
    if (!resolver) {
        return std::unexpected(std::move(resolver.error()));
    }
    Names names;
    for (const ModelFiles& files : models) {
        InsertUpper(names.models, files.geometry);
        InsertUpper(names.materialLibraries, files.materials);
        InsertUpper(names.textureDatabases, files.textures);
    }
    return LoadNamed(*resolver, {}, names, isMirrored);
}

Result<TrackAssets> TrackLoader::LoadNamed(const NameResolver& resolver, Handles<WorldAsset> worlds,
                                           const Names& names, bool isMirrored) {
    const Loading loading{.vfs = vfs_,
                          .pool = pool_,
                          .resolver = &resolver,
                          .isMirrored = isMirrored,
                          .unmirrored = &names.skyModels};
    Result<NameMap<AssetCache<GeometryAsset>::Handle>> models =
        LoadByName<GeometryAsset>(loading, models_, names.models, ".GDB", ParseGeometry, MirrorY);
    Result<NameMap<MaterialAsset>> materials = models.and_then([&](const auto&) {
        return LoadMaterials(loading, materialLibraries_, names.materialLibraries);
    });
    std::vector<std::string> missingTextures;
    Result<NameMap<AssetCache<ImageAsset>::Handle>> textures = materials.and_then([&](const auto&) {
        return LoadTextures(loading, textureDatabases_, images_, names.textureDatabases,
                            missingTextures);
    });
    Result<NameMap<AssetCache<MaterialAnimationAsset>::Handle>> animations =
        textures.and_then([&](const auto&) {
            return LoadByName<MaterialAnimationAsset>(loading, materialAnimations_,
                                                      names.materialAnimations, ".MAB",
                                                      ParseMaterialAnimation);
        });
    if (!animations) {
        return std::unexpected(std::move(animations.error()));
    }
    return TrackAssets{.worlds = std::move(worlds),
                       .models = std::move(*models),
                       .materials = std::move(*materials),
                       .textures = std::move(*textures),
                       .materialAnimations = std::move(*animations),
                       .missingTextures = std::move(missingTextures)};
}

void TrackLoader::Prune() {
    worlds_.Prune();
    models_.Prune();
    materialLibraries_.Prune();
    textureDatabases_.Prune();
    images_.Prune();
    materialAnimations_.Prune();
}

std::vector<std::string> ListRaceTracks(const Vfs& vfs) {
    std::vector<std::string> tracks;
    for (VfsEntry& entry : vfs.List(kRaceFolder).value_or(std::vector<VfsEntry>{})) {
        if (entry.kind != EntryKind::Directory) {
            continue;
        }
        const std::vector<VfsEntry> files = vfs.List(std::format("{}/{}", kRaceFolder, entry.name))
                                                .value_or(std::vector<VfsEntry>{});
        if (HasFile(files, kStartGrid) && HasFile(files, kCheckpoints)) {
            tracks.push_back(ToUpperAscii(entry.name));
        }
    }
    std::ranges::sort(tracks);
    return tracks;
}

std::string_view RaceTrackStem(std::string_view track) {
    return track.starts_with(kRaceFolderPrefix) ? track.substr(kRaceFolderPrefix.size()) : track;
}

void MergeTrackAssets(TrackAssets& into, TrackAssets from) {
    into.models.merge(from.models);
    into.materials.merge(from.materials);
    into.textures.merge(from.textures);
    into.materialAnimations.merge(from.materialAnimations);
    std::ranges::move(from.missingTextures, std::back_inserter(into.missingTextures));
}

}
