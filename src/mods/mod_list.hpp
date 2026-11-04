#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "assets/vfs.hpp"
#include "mods/mod.hpp"
#include "mods/mod_resolution.hpp"
#include "mods/mod_toggles.hpp"
#include "settings/registry.hpp"

namespace rr::mods {

/// Highest priority first, ties broken by id.
class ModList {
public:
    /// Skips a mod with a broken manifest, with a warning. A missing folder has no mods.
    [[nodiscard]] static ModList Discover(const std::filesystem::path& modsFolder);

    ModList() = default;

    [[nodiscard]] const std::vector<Mod>& All() const { return mods_; }
    [[nodiscard]] const std::vector<std::string>& SkippedMods() const { return skipped_; }

    [[nodiscard]] std::vector<bool> Toggles(const settings::Registry& registry) const;
    [[nodiscard]] ModResolution Resolve(const settings::Registry& registry) const;

    /// The mods that load, top first: on, with what they need, and not held back.
    [[nodiscard]] std::vector<const Mod*> Enabled(const settings::Registry& registry) const;

    /// Returns the keys that changed.
    std::vector<std::string> Apply(settings::Registry& registry,
                                   std::span<const ModSwitch> switches) const;

    /// Returns the keys that changed.
    std::vector<std::string> TurnOnRequired(settings::Registry& registry) const;

    void DeclareSettings(settings::Registry& registry) const;

    /// Mounts the enabled mods' `files/` above every layer mounted so far, in load order.
    void MountEnabled(assets::Vfs& vfs, const settings::Registry& registry) const;

private:
    ModList(std::vector<Mod> mods, std::vector<std::string> skipped)
        : mods_(std::move(mods)), skipped_(std::move(skipped)) {}

    std::vector<Mod> mods_;
    std::vector<std::string> skipped_;
};

}
