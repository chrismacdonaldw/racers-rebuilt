#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "mods/mod_manifest.hpp"
#include "settings/setting_spec.hpp"

namespace rr::mods {

class Mod {
public:
    Mod(ModManifest manifest, std::filesystem::path folder);

    [[nodiscard]] const ModManifest& Manifest() const { return manifest_; }
    [[nodiscard]] std::filesystem::path ManifestPath() const { return folder_ / "mod.toml"; }
    [[nodiscard]] std::filesystem::path FilesFolder() const { return folder_ / "files"; }
    [[nodiscard]] std::filesystem::path MusicFolder() const { return folder_ / "music"; }

    /// `mods.<id>.enabled`, viewing this mod's strings.
    [[nodiscard]] settings::SettingSpec<bool> EnabledSetting() const;

    /// Covers the paths and sizes of every file in the mod's folder, read now.
    [[nodiscard]] std::uint64_t ContentHash() const;

private:
    ModManifest manifest_;
    std::filesystem::path folder_;
    std::string enabledKey_;
};

}
