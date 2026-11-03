#include "app/gltf_models.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/geometry_database.hpp"
#include "assets/geometry_writer.hpp"
#include "assets/gltf_model.hpp"
#include "assets/skeleton_database.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kModelExtension = ".GDB";
constexpr std::string_view kGlbExtension = ".GLB";
constexpr std::string_view kSkeletonExtension = ".SDB";
constexpr std::string_view kDriverSkeleton = "PELVIS";

std::vector<std::string> SkeletonPaths(std::string_view model) {
    const std::string_view stem = model.substr(0, model.size() - kModelExtension.size());
    std::vector<std::string> paths{std::string(stem).append(kSkeletonExtension)};
    const std::size_t slash = stem.rfind('/');
    const std::string_view folder =
        slash == std::string_view::npos ? "" : stem.substr(0, slash + 1);
    if (stem.ends_with(kDriverSkeleton) && stem.size() - folder.size() > kDriverSkeleton.size()) {
        paths.push_back(std::string(folder).append(kDriverSkeleton).append(kSkeletonExtension));
    }
    return paths;
}

std::vector<std::string> JointNames(const assets::Vfs& vfs, std::string_view model) {
    for (const std::string& path : SkeletonPaths(model)) {
        if (!vfs.Contains(path)) {
            continue;
        }
        const Result<assets::SkeletonAsset> skeleton = vfs.ReadOriginal(path).and_then(
            [](const assets::FileData& file) { return assets::ParseSkeleton(file.Bytes()); });
        if (!skeleton) {
            log::Warn("Importing {} without joints because {} can't be read: {}", model, path,
                      skeleton.error().message);
            return {};
        }
        std::vector<std::string> names;
        for (const assets::SkeletonNode& node : skeleton->nodes) {
            names.push_back(node.name);
        }
        return names;
    }
    return {};
}

assets::GltfTarget TargetOf(const assets::Vfs& vfs, std::string_view model) {
    const Result<assets::GeometryAsset> original = vfs.ReadOriginal(model).and_then(
        [](const assets::FileData& file) { return assets::ParseGeometry(file.Bytes()); });
    return {.joints = JointNames(vfs, model), .scale = original ? original->scale : 1.0F};
}

Result<std::vector<std::byte>> BuildModel(const assets::Vfs& vfs, std::string_view model,
                                          const std::string& glb) {
    const assets::GltfTarget target = TargetOf(vfs, model);
    return vfs.ReadOriginal(glb)
        .and_then([&](const assets::FileData& file) {
            return assets::ImportGltfModel(file.Bytes(), target);
        })
        .and_then([](const assets::GltfModel& model) {
            return assets::WriteGeometry(model.geometry, model.vertices);
        });
}

}

std::optional<std::string> GlbSibling(std::string_view path) {
    if (!path.ends_with(kModelExtension)) {
        return std::nullopt;
    }
    path.remove_suffix(kModelExtension.size());
    return std::string(path).append(kGlbExtension);
}

void GltfModels::DeclareSettings(settings::Registry& registry) const {
    registry.Declare(kGltfModels);
}

void GltfModels::Attach(mods::HookRegistry& hooks, const settings::Registry& settings) {
    hooks.Subscribe<mods::AssetResolving>([&settings](mods::AssetResolving& event) {
        if (!settings.Get(kGltfModels) || event.request.generated) {
            return;
        }
        const std::optional<std::string> glb = GlbSibling(event.request.path);
        if (!glb || !event.vfs.Contains(*glb)) {
            return;
        }
        Result<std::vector<std::byte>> model = BuildModel(event.vfs, event.request.path, *glb);
        if (!model) {
            log::Warn("Keeping {} because {} can't be used: {}", event.request.path, *glb,
                      model.error().message);
            return;
        }
        event.request.generated = std::move(*model);
    });
}

}
