#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "mods/enhancement.hpp"
#include "settings/setting_spec.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kHqTextures{
    .key = "mods.hq_textures", .defaultValue = true, .label = "HQ textures", .category = "Mods"};

/// `path` is normalized. Only `.BMP` and `.TGA` images have one.
[[nodiscard]] std::optional<std::string> PngSibling(std::string_view path);

/// Reads a mod's PNG in place of the game image it shares a name with.
class HqTextures final : public mods::Enhancement {
public:
    [[nodiscard]] std::string_view Name() const override { return "HQ textures"; }
    [[nodiscard]] bool ChangesGameplay() const override { return false; }

    void DeclareSettings(settings::Registry& registry) const override;
    void Attach(mods::HookRegistry& hooks, const settings::Registry& settings) override;
};

}
