#include "app/extensions.hpp"

#include "app/free_camera.hpp"
#include "app/gltf_models.hpp"
#include "app/hq_textures.hpp"
#include "app/ldraw_driver_prints.hpp"
#include "app/ldraw_minifig_models.hpp"
#include "app/ldraw_piece_models.hpp"
#include "core/log.hpp"
#include "platform/data_folder.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kLdrawFolderName = "ldraw";

std::filesystem::path ShippedLdrawFolder() {
    const Result<std::filesystem::path> exeFolder = platform::ExeFolder();
    return (exeFolder ? *exeFolder : std::filesystem::path()) / kLdrawFolderName;
}

}

Extensions LoadExtensions(const std::filesystem::path& modsFolder,
                          const std::filesystem::path& overridesFolder) {
    Extensions extensions{.mods = mods::ModList::Discover(modsFolder),
                          .overrides = mods::OverrideFolder(overridesFolder),
                          .enhancements = {}};
    extensions.enhancements.push_back(std::make_unique<FreeCamera>());
    extensions.enhancements.push_back(std::make_unique<HqTextures>());
    extensions.enhancements.push_back(std::make_unique<GltfModels>());
    extensions.enhancements.push_back(std::make_unique<LdrawPieceModels>(ShippedLdrawFolder()));
    extensions.enhancements.push_back(std::make_unique<LdrawDriverPrints>(ShippedLdrawFolder()));
    extensions.enhancements.push_back(std::make_unique<LdrawMinifigModels>(ShippedLdrawFolder()));
    return extensions;
}

void DeclareExtensionSettings(const Extensions& extensions, settings::Registry& registry) {
    extensions.mods.DeclareSettings(registry);
    registry.Declare(mods::kOverridesSetting);
    for (const std::unique_ptr<mods::Enhancement>& enhancement : extensions.enhancements) {
        enhancement->DeclareSettings(registry);
    }
}

void MountExtensions(Extensions& extensions, assets::Vfs& vfs, const settings::Registry& registry) {
    extensions.gameLayers = vfs.LayerCount();
    extensions.mods.MountEnabled(vfs, registry);
    extensions.overrides.Mount(vfs, registry);
    log::Info("{} mods, {} enabled", extensions.mods.All().size(),
              extensions.mods.Enabled(registry).size());
}

void RemountExtensions(Extensions& extensions, assets::Vfs& vfs,
                       const settings::Registry& registry) {
    vfs.KeepLayers(extensions.gameLayers);
    MountExtensions(extensions, vfs, registry);
}

void AttachEnhancements(const Extensions& extensions, mods::HookRegistry& hooks, assets::Vfs& vfs,
                        const settings::Registry& settings) {
    mods::ResolveThroughHooks(vfs, hooks);
    for (const std::unique_ptr<mods::Enhancement>& enhancement : extensions.enhancements) {
        enhancement->Attach(hooks, settings);
        log::Info("enhancement: {}{}", enhancement->Name(),
                  enhancement->ChangesGameplay() ? ", changes gameplay" : "");
    }
}

}
