#pragma once

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "assets/ldraw_part.hpp"
#include "assets/vfs.hpp"
#include "mods/enhancement.hpp"
#include "settings/setting_spec.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kLdrawMinifigsSetting{
    .key = "enhancements.ldraw_minifigs",
    .defaultValue = false,
    .label = "LDraw minifigs (beta, very buggy)",
    .category = "Enhancements/Visuals",
    .tooltip = "Drivers can bend, stretch and clip in odd ways."};

class LdrawMinifigModels final : public mods::Enhancement {
public:
    explicit LdrawMinifigModels(std::filesystem::path folder) : library_(std::move(folder)) {}

    [[nodiscard]] std::string_view Name() const override { return "LDraw minifigs"; }
    [[nodiscard]] bool ChangesGameplay() const override { return false; }

    void DeclareSettings(settings::Registry& registry) const override;
    void Attach(mods::HookRegistry& hooks, const settings::Registry& settings) override;

    /// Thread-safe. Empty when `path` isn't a minifig file, a mod also has it, or it can't be
    /// fitted.
    [[nodiscard]] std::optional<std::vector<std::byte>> ModelFile(const assets::Vfs& vfs,
                                                                  std::string_view path);

private:
    std::mutex mutex_;
    assets::LdrawLibrary library_;
    std::unordered_map<std::string, std::optional<std::vector<std::byte>>> files_;
};

}
