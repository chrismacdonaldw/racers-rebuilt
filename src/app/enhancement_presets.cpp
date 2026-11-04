#include "app/enhancement_presets.hpp"

#include <string>
#include <string_view>

#include "app/audio_settings.hpp"
#include "app/cheat_settings.hpp"
#include "app/fix_settings.hpp"
#include "app/modifier_settings.hpp"
#include "app/race_settings.hpp"
#include "app/video_settings.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kMenu = "Enhancements";

}

std::vector<settings::SettingPreset> EnhancementPresets() {
    const std::string gameplay(kRespawn.category);
    return {
        {.label = "Original",
         .menu = std::string(kMenu),
         .resets = {gameplay, std::string(kCheatsCategory), std::string(kModifiersCategory),
                    std::string(kFixesCategory)},
         .values = {{std::string(kRespawn.key), false},
                    {std::string(kSmoothTexturesSetting.key), false},
                    {std::string(kAnisotropicFilteringSetting.key), false},
                    {std::string(kFarCarsSetting.key), false},
                    {std::string(kFarScenerySetting.key), false},
                    {std::string(kWidescreenSetting.key), false},
                    {std::string(kPreventClipping.key), false}}},
        {.label = "Enhanced",
         .menu = std::string(kMenu),
         .resets = {gameplay},
         .values = {{std::string(kPhysicsAi.key), true},
                    {std::string(kSmoothTexturesSetting.key), true},
                    {std::string(kAnisotropicFilteringSetting.key), true},
                    {std::string(kChampionCars.key), true},
                    {std::string(kFarCarsSetting.key), true},
                    {std::string(kFarScenerySetting.key), true},
                    {std::string(kWidescreenSetting.key), true},
                    {std::string(kPreventClipping.key), true}}},
    };
}

}
