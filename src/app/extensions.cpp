#include "app/extensions.hpp"

#include "app/free_camera.hpp"
#include "core/log.hpp"

namespace rr::app {

Extensions LoadExtensions(const std::filesystem::path& modsFolder) {
    Extensions extensions{.mods = mods::ModList::Discover(modsFolder), .enhancements = {}};
    extensions.enhancements.push_back(std::make_unique<FreeCamera>());
    return extensions;
}

void DeclareExtensionSettings(const Extensions& extensions, settings::Registry& registry) {
    extensions.mods.DeclareSettings(registry);
    for (const std::unique_ptr<mods::Enhancement>& enhancement : extensions.enhancements) {
        enhancement->DeclareSettings(registry);
    }
}

void AttachEnhancements(const Extensions& extensions, mods::HookRegistry& hooks,
                        const settings::Registry& settings) {
    for (const std::unique_ptr<mods::Enhancement>& enhancement : extensions.enhancements) {
        enhancement->Attach(hooks, settings);
        log::Info("enhancement: {}{}", enhancement->Name(),
                  enhancement->ChangesGameplay() ? ", changes gameplay" : "");
    }
}

}
