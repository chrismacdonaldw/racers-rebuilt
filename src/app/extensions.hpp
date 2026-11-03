#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "assets/vfs.hpp"
#include "mods/enhancement.hpp"
#include "mods/hook_registry.hpp"
#include "mods/mod_list.hpp"
#include "mods/override_folder.hpp"
#include "settings/registry.hpp"

namespace rr::app {

struct Extensions {
    mods::ModList mods;
    mods::OverrideFolder overrides;
    std::vector<std::unique_ptr<mods::Enhancement>> enhancements;
};

/// Built-in enhancements are registered here.
[[nodiscard]] Extensions LoadExtensions(const std::filesystem::path& modsFolder,
                                        const std::filesystem::path& overridesFolder);

void DeclareExtensionSettings(const Extensions& extensions, settings::Registry& registry);

void AttachEnhancements(const Extensions& extensions, mods::HookRegistry& hooks,
                        const settings::Registry& settings);

}
