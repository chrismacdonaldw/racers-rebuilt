#include "app/enhancement_presets.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

#include "app/audio_settings.hpp"
#include "app/cheat_settings.hpp"
#include "app/fix_settings.hpp"
#include "app/ldraw_driver_prints.hpp"
#include "app/ldraw_piece_models.hpp"
#include "app/menu/builder_settings.hpp"
#include "app/race_settings.hpp"
#include "app/video_settings.hpp"
#include "app/warp_settings.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

settings::Registry DeclaredRegistry() {
    settings::Registry registry;
    DeclareRaceSettings(registry);
    DeclareCheatSettings(registry);
    DeclareVideoSettings(registry);
    DeclareAudioSettings(registry);
    DeclareFixSettings(registry);
    DeclareWarpSettings(registry);
    registry.Declare(kLdrawPiecesSetting);
    registry.Declare(kLdrawPrintsSetting);
    registry.Declare(kEnhancedBuilder);
    return registry;
}

}

TEST_CASE("every enhancement preset names declared settings") {
    const settings::Registry registry = DeclaredRegistry();
    for (const settings::SettingPreset& preset : EnhancementPresets()) {
        CHECK(preset.menu == "Enhancements");
        for (const auto& [key, value] : preset.values) {
            CHECK(registry.AllDeclarations().contains(key));
        }
    }
}

TEST_CASE("the default preset clears the cheats and the enhanced one keeps them") {
    settings::Registry registry = DeclaredRegistry();
    const std::vector<settings::SettingPreset> presets = EnhancementPresets();
    REQUIRE(presets.size() == 2);
    const settings::SettingPreset& defaults = presets[0];
    const settings::SettingPreset& enhanced = presets[1];
    const settings::SettingSpec<bool>& noWheels = kCheatToggles.back().setting;
    registry.Set(noWheels, true);
    registry.Set(kAiPace, kMaxAiPace);

    static_cast<void>(ApplyPreset(registry, enhanced));
    CHECK(registry.Get(kPhysicsAi));
    CHECK(registry.Get(kRespawn));
    CHECK(registry.Get(kAiPace) == kDefaultAiPace);
    CHECK(registry.Get(noWheels));
    CHECK(registry.Get(kChampionCars));
    CHECK(IsPresetActive(registry, enhanced));
    CHECK_FALSE(IsPresetActive(registry, defaults));

    static_cast<void>(ApplyPreset(registry, defaults));
    CHECK_FALSE(registry.Get(kPhysicsAi));
    CHECK_FALSE(registry.Get(kRespawn));
    CHECK_FALSE(registry.Get(noWheels));
    CHECK_FALSE(registry.Get(kChampionCars));
    CHECK(IsPresetActive(registry, defaults));
    CHECK_FALSE(IsPresetActive(registry, enhanced));
}

TEST_CASE("the default preset turns smoothing, filtering and clipping prevention off") {
    settings::Registry registry = DeclaredRegistry();
    const std::vector<settings::SettingPreset> presets = EnhancementPresets();
    REQUIRE(presets.size() == 2);

    CHECK_FALSE(registry.Get(kAnisotropicFilteringSetting));
    static_cast<void>(ApplyPreset(registry, presets[0]));
    CHECK_FALSE(registry.Get(kSmoothTexturesSetting));
    CHECK_FALSE(registry.Get(kAnisotropicFilteringSetting));
    CHECK_FALSE(registry.Get(kPreventClipping));
    static_cast<void>(ApplyPreset(registry, presets[1]));
    CHECK(registry.Get(kSmoothTexturesSetting));
    CHECK(registry.Get(kAnisotropicFilteringSetting));
    CHECK(registry.Get(kPreventClipping));
}

TEST_CASE("every enhancement defaults off and the default preset matches a fresh install") {
    const settings::Registry registry = DeclaredRegistry();
    const std::vector<settings::SettingPreset> presets = EnhancementPresets();
    REQUIRE(presets.size() == 2);
    CHECK(presets[0].label == "Default");
    CHECK(IsPresetActive(registry, presets[0]));
    CHECK(std::ranges::none_of(presets[1].values, [&](const auto& value) {
        return registry.AllDeclarations().find(value.first)->second.defaultValue == value.second;
    }));
}

TEST_CASE("only the enhanced preset turns on widescreen, HUD width, far models and the palette") {
    settings::Registry registry = DeclaredRegistry();
    const std::vector<settings::SettingPreset> presets = EnhancementPresets();
    REQUIRE(presets.size() == 2);
    CHECK_FALSE(registry.IsAvailable(kHudWidthSetting.key));
    static_cast<void>(ApplyPreset(registry, presets[1]));
    CHECK(registry.Get(kWidescreenSetting));
    CHECK(registry.Get(kHudWidthSetting) == "21:9");
    CHECK(registry.Get(kWidescreenCutscenesSetting));
    CHECK(registry.IsAvailable(kHudWidthSetting.key));
    CHECK(registry.Get(kFarCarsSetting));
    CHECK(registry.Get(kFarScenerySetting));
    CHECK(registry.Get(kEnhancedBuilder));
    static_cast<void>(ApplyPreset(registry, presets[0]));
    CHECK_FALSE(registry.Get(kWidescreenSetting));
    CHECK(registry.Get(kHudWidthSetting) == "Window");
    CHECK_FALSE(registry.Get(kWidescreenCutscenesSetting));
    CHECK_FALSE(registry.Get(kEnhancedBuilder));
}

TEST_CASE("quick configs load the user's presets folder beside the shipped presets") {
    const test::TempFolder folder;
    const settings::Registry registry = DeclaredRegistry();
    const settings::QuickConfigs empty = LoadQuickConfigs(registry, folder.Path());
    CHECK(empty.User().empty());
    CHECK(empty.Shipped().size() == 2);

    std::filesystem::create_directories(folder.Path() / "presets");
    std::ofstream(folder.Path() / "presets" / "mine.toml", std::ios::binary)
        << "name = \"Mine\"\n[settings.race]\nrespawn = true\n";
    const settings::QuickConfigs configs = LoadQuickConfigs(registry, folder.Path());
    REQUIRE(configs.User().size() == 1);
    CHECK(configs.User()[0].preset.label == "Mine");
}

TEST_CASE("the warp enhancements are off by default, on when enhanced and off again by default") {
    settings::Registry registry = DeclaredRegistry();
    const std::vector<settings::SettingPreset> presets = EnhancementPresets();
    REQUIRE(presets.size() == 2);

    CHECK(WarpRulesFrom(registry) == sim::WarpRules{});
    static_cast<void>(ApplyPreset(registry, presets[1]));
    CHECK(WarpRulesFrom(registry) ==
          sim::WarpRules{
              .isFair = true, .catchesUp = true, .aimsLanding = true, .endsWhenTakenBack = true});
    static_cast<void>(ApplyPreset(registry, presets[0]));
    CHECK(WarpRulesFrom(registry) == sim::WarpRules{});
}

}
