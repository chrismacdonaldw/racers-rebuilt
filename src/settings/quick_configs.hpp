#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "settings/registry.hpp"
#include "settings/setting_preset.hpp"

namespace rr::settings {

struct UserQuickConfig {
    std::string slug;
    SettingPreset preset;
};

/// User configs are `<folder>/<slug>.toml`, listed in the menu of the first shipped preset.
class QuickConfigs {
public:
    QuickConfigs(std::vector<SettingPreset> shipped, std::filesystem::path folder);

    void Reload(const Registry& registry);

    [[nodiscard]] std::span<const SettingPreset> Shipped() const { return shipped_; }
    [[nodiscard]] std::span<const UserQuickConfig> User() const { return user_; }
    [[nodiscard]] const UserQuickConfig* Find(std::string_view name) const;

    /// Replaces the config whose file stem has the same slug as `name`.
    [[nodiscard]] Result<void> Save(const Registry& registry, std::string_view name);
    [[nodiscard]] Result<void> Remove(const Registry& registry, std::string_view slug);

private:
    [[nodiscard]] std::filesystem::path PathOf(std::string_view slug) const;

    std::vector<SettingPreset> shipped_;
    std::filesystem::path folder_;
    std::vector<UserQuickConfig> user_;
};

}
