#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "assets/vfs.hpp"
#include "mods/mod_manifest.hpp"
#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"

namespace rr::mods {

class Mod {
public:
    Mod(ModManifest manifest, std::filesystem::path folder);

    [[nodiscard]] const ModManifest& Manifest() const { return manifest_; }
    [[nodiscard]] std::filesystem::path FilesFolder() const { return folder_ / "files"; }
    [[nodiscard]] std::filesystem::path MusicFolder() const { return folder_ / "music"; }

    /// `mods.<id>.enabled`, viewing this mod's strings.
    [[nodiscard]] settings::SettingSpec<bool> EnabledSetting() const;

private:
    ModManifest manifest_;
    std::filesystem::path folder_;
    std::string enabledKey_;
};

/// Highest priority first, ties broken by id.
class ModList {
public:
    /// Skips a mod with a broken manifest, with a warning. A missing folder has no mods.
    [[nodiscard]] static ModList Discover(const std::filesystem::path& modsFolder);

    ModList() = default;

    [[nodiscard]] const std::vector<Mod>& All() const { return mods_; }
    [[nodiscard]] std::vector<const Mod*> Enabled(const settings::Registry& registry) const;

    void DeclareSettings(settings::Registry& registry) const;

    /// Mounts each enabled mod's `files/` above every layer mounted so far.
    void MountEnabled(assets::Vfs& vfs, const settings::Registry& registry) const;

private:
    explicit ModList(std::vector<Mod> mods) : mods_(std::move(mods)) {}

    std::vector<Mod> mods_;
};

}
