#include "mods/mod.hpp"

#include <format>
#include <utility>

#include "mods/folder_hash.hpp"

namespace rr::mods {

Mod::Mod(ModManifest manifest, std::filesystem::path folder)
    : manifest_(std::move(manifest)),
      folder_(std::move(folder)),
      enabledKey_(std::format("mods.{}.enabled", manifest_.id)) {}

settings::SettingSpec<bool> Mod::EnabledSetting() const {
    return {.key = enabledKey_,
            .defaultValue = manifest_.isEnabledByDefault,
            .label = manifest_.name,
            .category = "Mods"};
}

std::uint64_t Mod::ContentHash() const {
    return HashStamps(FileStamps(folder_));
}

}
