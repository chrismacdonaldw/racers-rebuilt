#include "assets/track_loader.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <optional>
#include <set>
#include <span>
#include <utility>

#include "assets/bmp_image.hpp"
#include "assets/game_path.hpp"
#include "assets/geometry_database.hpp"
#include "assets/tga_image.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kCommonFolder = "GAMEDATA/COMMON";
constexpr std::size_t kBytesPerPixel = 4;

using NameSet = std::set<std::string, std::less<>>;

template <typename T>
using Handles = std::vector<typename AssetCache<T>::Handle>;

template <typename T>
using Parser = std::function<Result<T>(std::size_t, std::span<const std::byte>)>;

struct Request {
    std::string key;
    std::string path;
};

template <typename T>
Result<Handles<T>> LoadAll(const Vfs& vfs, LoaderPool& pool, AssetCache<T>& cache,
                           std::span<const Request> requests, const Parser<T>& parse) {
    Handles<T> handles(requests.size());
    std::vector<std::size_t> missing;
    for (std::size_t i = 0; i < requests.size(); ++i) {
        handles[i] = cache.Find(requests[i].key);
        if (!handles[i]) {
            missing.push_back(i);
        }
    }
    std::vector<std::optional<Result<T>>> parsed(missing.size());
    pool.ForEach(missing.size(), [&](std::size_t job) {
        const std::size_t index = missing[job];
        parsed[job] = vfs.Read(requests[index].path).and_then([&](const FileData& data) {
            return parse(index, data.Bytes());
        });
    });
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

struct Loading {
    const Vfs* vfs = nullptr;
    LoaderPool* pool = nullptr;
    const NameResolver* resolver = nullptr;
};

template <typename T>
Result<NameMap<typename AssetCache<T>::Handle>> LoadByName(
    const Loading& loading, AssetCache<T>& cache, const NameSet& names, std::string_view extension,
    Result<T> (*parse)(std::span<const std::byte>)) {
    Result<Handles<T>> handles =
        loading.resolver->ResolveAll(names, extension)
            .and_then([&](const std::vector<Request>& requests) {
                return LoadAll<T>(*loading.vfs, *loading.pool, cache, requests,
                                  [parse](std::size_t, std::span<const std::byte> bytes) {
                                      return parse(bytes);
                                  });
            });
    if (!handles) {
        return std::unexpected(std::move(handles.error()));
    }
    NameMap<typename AssetCache<T>::Handle> byName;
    auto handle = handles->begin();
    for (const std::string& name : names) {
        byName.emplace(name, std::move(*handle++));
    }
    return byName;
}

void FlipRows(ImageAsset& image) {
    const std::size_t rowBytes = std::size_t{image.widthPx} * kBytesPerPixel;
    const std::span<std::byte> pixels = image.rgba;
    for (std::size_t top = 0, bottom = image.heightPx; top + 1 < bottom; ++top, --bottom) {
        std::ranges::swap_ranges(pixels.subspan(top * rowBytes, rowBytes),
                                 pixels.subspan((bottom - 1) * rowBytes, rowBytes));
    }
}

Result<ImageAsset> DecodeTexture(std::span<const std::byte> bytes,
                                 const TextureDefinition& definition) {
    Result<ImageAsset> image =
        definition.fileKind == TextureFileKind::Bmp ? DecodeBmp(bytes) : DecodeTga(bytes);
    if (image && definition.colorKey) {
        ApplyColorKey(*image, *definition.colorKey);
    }
    if (image && definition.flipsVertically) {
        FlipRows(*image);
    }
    return image;
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
    return LoadByName<std::vector<MaterialAsset>>(loading, cache, names, ".MDB",
                                                  ParseMaterialLibrary)
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
        LoadAll<ImageAsset>(*loading.vfs, *loading.pool, cache, requests,
                            [&](std::size_t index, std::span<const std::byte> bytes) {
                                return DecodeTexture(bytes, requested[index]->second);
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
    return LoadByName<std::vector<TextureDefinition>>(loading, databaseCache, names, ".TDB",
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

// NOLINTNEXTLINE(bugprone-exception-escape)
struct WorldNames {
    NameSet models;
    NameSet materialLibraries;
    NameSet textureDatabases;
};

WorldNames CollectNames(const Handles<WorldAsset>& worlds) {
    WorldNames names;
    for (const AssetCache<WorldAsset>::Handle& world : worlds) {
        for (const std::string& model : world->models) {
            names.models.insert(ToUpperAscii(model));
        }
        for (const std::string& library : world->materialLibraries) {
            names.materialLibraries.insert(ToUpperAscii(library));
        }
        for (const std::string& database : world->textureLists) {
            names.textureDatabases.insert(ToUpperAscii(database));
        }
    }
    return names;
}

}

Result<TrackAssets> TrackLoader::Load(std::string_view folder) {
    Result<NameResolver> resolver = NameResolver::Create(*vfs_, folder);
    if (!resolver) {
        return std::unexpected(std::move(resolver.error()));
    }
    const Loading loading{.vfs = vfs_, .pool = pool_, .resolver = &*resolver};
    Result<Handles<WorldAsset>> worlds = LoadAll<WorldAsset>(
        *vfs_, *pool_, worlds_, resolver->LocalFilesWithExtension(".WDB"),
        [](std::size_t, std::span<const std::byte> bytes) { return ParseWorld(bytes); });
    if (!worlds) {
        return std::unexpected(std::move(worlds.error()));
    }
    const WorldNames names = CollectNames(*worlds);
    Result<NameMap<AssetCache<GeometryAsset>::Handle>> models =
        LoadByName<GeometryAsset>(loading, models_, names.models, ".GDB", ParseGeometry);
    Result<NameMap<MaterialAsset>> materials = models.and_then([&](const auto&) {
        return LoadMaterials(loading, materialLibraries_, names.materialLibraries);
    });
    std::vector<std::string> missingTextures;
    Result<NameMap<AssetCache<ImageAsset>::Handle>> textures = materials.and_then([&](const auto&) {
        return LoadTextures(loading, textureDatabases_, images_, names.textureDatabases,
                            missingTextures);
    });
    if (!textures) {
        return std::unexpected(std::move(textures.error()));
    }
    return TrackAssets{.worlds = std::move(*worlds),
                       .models = std::move(*models),
                       .materials = std::move(*materials),
                       .textures = std::move(*textures),
                       .missingTextures = std::move(missingTextures)};
}

void TrackLoader::Prune() {
    worlds_.Prune();
    models_.Prune();
    materialLibraries_.Prune();
    textureDatabases_.Prune();
    images_.Prune();
}

}
