#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "mods/enhancement.hpp"
#include "settings/setting_spec.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kGltfModels{
    .key = "mods.gltf_models", .defaultValue = true, .label = "glTF models", .category = "Mods"};

/// `path` is normalized. Only `.GDB` models and `.ADB` animations have one.
[[nodiscard]] std::optional<std::string> GlbSibling(std::string_view path);

/// `model` is a normalized `.GDB` path. The first of these that exists holds its joints.
[[nodiscard]] std::vector<std::string> SkeletonPaths(std::string_view model);

/// Builds a game model from a mod's `.glb` in place of the `.GDB` it shares a name with.
class GltfModels final : public mods::Enhancement {
public:
    [[nodiscard]] std::string_view Name() const override { return "glTF models"; }
    [[nodiscard]] bool ChangesGameplay() const override { return false; }

    void DeclareSettings(settings::Registry& registry) const override;
    void Attach(mods::HookRegistry& hooks, const settings::Registry& settings) override;
};

}
