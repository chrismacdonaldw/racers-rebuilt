#include "app/enhancement_presets.hpp"

#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "app/audio_settings.hpp"
#include "app/fix_settings.hpp"
#include "app/ldraw_piece_models.hpp"
#include "app/menu/builder_settings.hpp"
#include "app/race_settings.hpp"
#include "app/video_settings.hpp"
#include "app/warp_settings.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kMenu = "Enhancements";
constexpr std::string_view kPresetsFolderName = "presets";

constexpr std::array kEnhancedSettings{&kPhysicsAi,
                                       &kAiBraking,
                                       &kAiPassing,
                                       &kAiUnsticking,
                                       &kAiAiming,
                                       &kAiSkill,
                                       &kAiFairCatchUp,
                                       &kRespawn,
                                       &kChampionCars,
                                       &kSmoothTexturesSetting,
                                       &kAnisotropicFilteringSetting,
                                       &kFarCarsSetting,
                                       &kFarScenerySetting,
                                       &kWidescreenSetting,
                                       &kWidescreenCutscenesSetting,
                                       &kPreventClipping,
                                       &kBetterWarp,
                                       &kFairWarp,
                                       &kCatchUpWarp,
                                       &kStuckWarpFix,
                                       &kLdrawPiecesSetting,
                                       &kEnhancedBuilder};

}

std::vector<settings::SettingPreset> EnhancementPresets() {
    settings::SettingPreset defaults{
        .label = "Default", .menu = std::string(kMenu), .resets = {std::string(kMenu)}};
    settings::SettingPreset enhanced{.label = "Enhanced",
                                     .menu = std::string(kMenu),
                                     .resets = {std::string(kRespawn.category)}};
    for (const settings::SettingSpec<bool>* spec : kEnhancedSettings) {
        enhanced.values.emplace_back(std::string(spec->key), true);
        if (!spec->category.starts_with(kMenu)) {
            defaults.values.emplace_back(std::string(spec->key), spec->defaultValue);
        }
    }
    const std::string hudWidth(kHudWidthSetting.key);
    enhanced.values.emplace_back(hudWidth, std::string(kHudWidthChoices[1]));
    defaults.values.emplace_back(hudWidth, std::string(kHudWidthSetting.defaultValue));
    return {std::move(defaults), std::move(enhanced)};
}

settings::QuickConfigs LoadQuickConfigs(const settings::Registry& registry,
                                        const std::filesystem::path& dataFolder) {
    settings::QuickConfigs configs(EnhancementPresets(), dataFolder / kPresetsFolderName);
    configs.Reload(registry);
    return configs;
}

}
