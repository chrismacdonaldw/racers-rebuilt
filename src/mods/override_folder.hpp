#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <utility>

#include "assets/vfs.hpp"
#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"

namespace rr::mods {

inline constexpr settings::SettingSpec<bool> kOverridesSetting{.key = "mods.overrides",
                                                               .defaultValue = true,
                                                               .label = "Loose file overrides",
                                                               .category = "Mods"};

/// Loose files at game paths, mounted above every mod with no manifest.
class OverrideFolder {
public:
    OverrideFolder() = default;
    explicit OverrideFolder(std::filesystem::path folder) : folder_(std::move(folder)) {}

    /// Does nothing when the setting is off or the folder is missing.
    void Mount(assets::Vfs& vfs, const settings::Registry& registry);

    /// Covers the mounted files' paths and sizes; empty when nothing was mounted.
    [[nodiscard]] std::optional<std::uint64_t> Hash() const { return hash_; }

private:
    std::filesystem::path folder_;
    std::optional<std::uint64_t> hash_;
};

}
