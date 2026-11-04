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
#include "mods/enhancement.hpp"
#include "settings/setting_spec.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kLdrawPiecesSetting{
    .key = "enhancements.ldraw_pieces",
    .defaultValue = false,
    .label = "LDraw pieces",
    .category = "Enhancements/Visuals"};

/// Answers piece model reads no layer has with LDraw parts from `folder`, built once per piece.
class LdrawPieceModels final : public mods::Enhancement {
public:
    explicit LdrawPieceModels(std::filesystem::path folder) : library_(std::move(folder)) {}

    [[nodiscard]] std::string_view Name() const override { return "LDraw pieces"; }
    [[nodiscard]] bool ChangesGameplay() const override { return false; }

    void DeclareSettings(settings::Registry& registry) const override;
    void Attach(mods::HookRegistry& hooks, const settings::Registry& settings) override;

    /// Thread-safe. Empty when the piece has no LDraw part or its part can't be loaded.
    [[nodiscard]] std::optional<std::vector<std::byte>> ModelFile(std::string_view pieceName);

private:
    std::mutex mutex_;
    assets::LdrawLibrary library_;
    std::unordered_map<std::string, std::optional<std::vector<std::byte>>> files_;
};

}
