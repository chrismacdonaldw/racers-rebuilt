#include "app/extensions.hpp"

#include "app/free_camera.hpp"
#include "core/log.hpp"

namespace rr::app {

Extensions LoadExtensions(const std::filesystem::path& modsFolder,
                          const std::filesystem::path& overridesFolder) {
    Extensions extensions{.mods = mods::ModList::Discover(modsFolder),
                          .overrides = mods::OverrideFolder(overridesFolder),
                          .enhancements = {}};
    extensions.enhancements.push_back(std::make_unique<FreeCamera>());
    return extensions;
}

void DeclareExtensionSettings(const Extensions& extensions, settings::Registry& registry) {
    extensions.mods.DeclareSettings(registry);
    registry.Declare(mods::kOverridesSetting);
    for (const std::unique_ptr<mods::Enhancement>& enhancement : extensions.enhancements) {
        enhancement->DeclareSettings(registry);
    }
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
