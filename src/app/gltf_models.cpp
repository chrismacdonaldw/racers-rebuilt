#include "app/gltf_models.hpp"

#include <cstddef>
#include <utility>
#include <vector>

#include "assets/geometry_writer.hpp"
#include "assets/gltf_model.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kModelExtension = ".GDB";
constexpr std::string_view kGlbExtension = ".GLB";

Result<std::vector<std::byte>> BuildModel(const assets::Vfs& vfs, const std::string& glb) {
    return vfs.ReadOriginal(glb)
        .and_then(
            [](const assets::FileData& file) { return assets::ImportGltfModel(file.Bytes()); })
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
        Result<std::vector<std::byte>> model = BuildModel(event.vfs, *glb);
        if (!model) {
            log::Warn("Keeping {} because {} can't be used: {}", event.request.path, *glb,
                      model.error().message);
            return;
        }
        event.request.generated = std::move(*model);
    });
}

}
