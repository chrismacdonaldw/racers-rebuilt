#include "app/model_export.hpp"

#include <format>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "app/gltf_models.hpp"
#include "app/image_dump.hpp"
#include "assets/animation_database.hpp"
#include "assets/game_path.hpp"
#include "assets/geometry_database.hpp"
#include "assets/gltf_export.hpp"
#include "assets/part_library.hpp"
#include "assets/skeleton_database.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kModelExtension = ".GDB";
constexpr std::string_view kAnimationExtension = ".ADB";
constexpr std::string_view kSkeletonExtension = ".SDB";
constexpr std::string_view kHeadsExtension = ".GCB";

struct ExportCounts {
    std::size_t exported = 0;
    std::size_t failed = 0;
};

template <typename Asset>
using Parser = Result<Asset> (*)(std::span<const std::byte>);

template <typename Asset>
Result<std::optional<Asset>> ReadGameAsset(const assets::Vfs& vfs, std::size_t layer,
                                           const std::string& path, Parser<Asset> parse) {
    Result<std::optional<assets::FileData>> data = vfs.ReadLayer(layer, path);
    if (!data) {
        return std::unexpected(std::move(data.error()));
    }
    if (!*data) {
        return std::nullopt;
    }
    return parse((*data)->Bytes()).transform([](Asset asset) {
        return std::optional(std::move(asset));
    });
}

template <typename Asset>
std::optional<Asset> FirstGameAsset(const assets::Vfs& vfs, std::size_t layer,
                                    const std::vector<std::string>& paths, Parser<Asset> parse) {
    for (const std::string& path : paths) {
        Result<std::optional<Asset>> asset = ReadGameAsset(vfs, layer, path, parse);
        if (!asset) {
            log::Warn("Exporting without {} because it can't be read: {}", path,
                      asset.error().message);
            return std::nullopt;
        }
        if (*asset) {
            return std::move(**asset);
        }
    }
    return std::nullopt;
}

std::vector<std::string> AnimationPaths(const std::vector<std::string>& skeletons) {
    std::vector<std::string> paths;
    paths.reserve(skeletons.size());
    for (const std::string& skeleton : skeletons) {
        paths.push_back(skeleton.substr(0, skeleton.size() - kSkeletonExtension.size())
                            .append(kAnimationExtension));
    }
    return paths;
}

Result<std::vector<std::byte>> ExportModel(const assets::Vfs& vfs, std::size_t layer,
                                           const std::string& model,
                                           const assets::GeometryAsset& geometry) {
    const std::vector<std::string> skeletons = SkeletonPaths(model);
    const std::optional<assets::SkeletonAsset> skeleton =
        FirstGameAsset<assets::SkeletonAsset>(vfs, layer, skeletons, assets::ParseSkeleton);
    std::optional<assets::AnimationAsset> animations;
    if (skeleton) {
        animations = FirstGameAsset<assets::AnimationAsset>(vfs, layer, AnimationPaths(skeletons),
                                                            assets::ParseAnimations);
    }
    assets::GltfSource source{.geometry = &geometry,
                              .skeleton = skeleton ? &*skeleton : nullptr,
                              .animations = animations ? &*animations : nullptr};
    Result<std::vector<std::byte>> glb = assets::ExportGltf(source);
    if (!glb && source.animations != nullptr) {
        log::Warn("Exporting {} without animations: {}", model, glb.error().message);
        source.animations = nullptr;
        glb = assets::ExportGltf(source);
    }
    return glb;
}

Result<void> WriteBytes(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return Fail(ErrorCode::Io, std::format("can't create {}: {}",
                                               Utf8FromPath(path.parent_path()), error.message()));
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), std::ssize(bytes));
    if (!file) {
        return Fail(ErrorCode::Io, std::format("can't write {}", Utf8FromPath(path)));
    }
    return {};
}

Result<bool> ExportHeadParts(const assets::Vfs& vfs, std::size_t layer,
                             const std::filesystem::path& folder, const std::string& gamePath) {
    Result<std::optional<assets::PartLibrary>> library =
        ReadGameAsset<assets::PartLibrary>(vfs, layer, gamePath, assets::ParsePartLibrary);
    if (!library) {
        log::Warn("Skipping {}: {}", gamePath, library.error().message);
        return std::unexpected(std::move(library.error()));
    }
    if (!*library) {
        return false;
    }
    const std::string_view stem =
        std::string_view(gamePath).substr(0, gamePath.size() - kHeadsExtension.size());
    for (const assets::LibraryPart& part : (*library)->parts) {
        const Result<std::vector<std::byte>> glb =
            assets::BuildPartGeometry(**library, part.name, 1.0F)
                .and_then([](const assets::GeometryAsset& geometry) {
                    return assets::ExportGltf({.geometry = &geometry});
                });
        if (!glb) {
            log::Warn("Skipping {} in {}: {}", part.name, gamePath, glb.error().message);
            continue;
        }
        const std::string path = std::format("{}/{}.GLB", stem, assets::ToUpperAscii(part.name));
        if (Result<void> written = WriteBytes(folder / PathFromUtf8(path), *glb); !written) {
            return std::unexpected(std::move(written.error()));
        }
    }
    return true;
}

Result<bool> ExportGameFile(const assets::Vfs& vfs, std::size_t layer,
                            const std::filesystem::path& folder, const std::string& gamePath) {
    if (gamePath.ends_with(kHeadsExtension)) {
        return ExportHeadParts(vfs, layer, folder, gamePath);
    }
    if (!gamePath.ends_with(kModelExtension)) {
        return false;
    }
    Result<std::optional<assets::GeometryAsset>> geometry =
        ReadGameAsset<assets::GeometryAsset>(vfs, layer, gamePath, assets::ParseGeometry);
    if (geometry && !*geometry) {
        return false;
    }
    Result<std::vector<std::byte>> glb =
        geometry.and_then([&](const std::optional<assets::GeometryAsset>& found) {
            return ExportModel(vfs, layer, gamePath, *found);
        });
    if (!glb) {
        log::Warn("Skipping {}: {}", gamePath, glb.error().message);
        return std::unexpected(std::move(glb.error()));
    }
    return WriteBytes(folder / PathFromUtf8(GlbSibling(gamePath).value_or(gamePath)), *glb)
        .transform([] { return true; });
}

}

Result<void> ExportGameModels(const assets::Vfs& vfs, std::size_t gameLayer,
                              const std::filesystem::path& folder) {
    ExportCounts counts;
    Result<void> walked = ForEachGameFile(vfs, [&](const std::string& gamePath) -> Result<void> {
        const Result<bool> exported = ExportGameFile(vfs, gameLayer, folder, gamePath);
        if (!exported && exported.error().code == ErrorCode::Io) {
            return std::unexpected(exported.error());
        }
        if (!exported) {
            ++counts.failed;
        } else if (*exported) {
            ++counts.exported;
        }
        return {};
    });
    if (walked) {
        log::Info("exported {} models to {}, {} failed", counts.exported, Utf8FromPath(folder),
                  counts.failed);
    }
    return walked;
}

}
