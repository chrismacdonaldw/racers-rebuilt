#include "app/enhancement_presets.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "app/cheat_settings.hpp"
#include "app/race_settings.hpp"
#include "app/video_settings.hpp"

namespace rr::app {
namespace {

settings::Registry DeclaredRegistry() {
    settings::Registry registry;
    DeclareRaceSettings(registry);
    DeclareCheatSettings(registry);
    DeclareVideoSettings(registry);
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

TEST_CASE("the original preset clears the cheats and the enhanced one keeps them") {
    settings::Registry registry = DeclaredRegistry();
    const std::vector<settings::SettingPreset> presets = EnhancementPresets();
    REQUIRE(presets.size() == 2);
    const settings::SettingPreset& original = presets[0];
    const settings::SettingPreset& enhanced = presets[1];
    const settings::SettingSpec<bool>& noWheels = kCheatToggles.back().setting;
    registry.Set(noWheels, true);
    registry.Set(kAiPace, kMaxAiPace);

    static_cast<void>(ApplyPreset(registry, enhanced));
    CHECK(registry.Get(kPhysicsAi));
    CHECK(registry.Get(kRespawn));
    CHECK(registry.Get(kAiPace) == kDefaultAiPace);
    CHECK(registry.Get(noWheels));
    CHECK(IsPresetActive(registry, enhanced));
    CHECK_FALSE(IsPresetActive(registry, original));

    static_cast<void>(ApplyPreset(registry, original));
    CHECK_FALSE(registry.Get(kPhysicsAi));
    CHECK_FALSE(registry.Get(kRespawn));
    CHECK_FALSE(registry.Get(noWheels));
    CHECK(IsPresetActive(registry, original));
    CHECK_FALSE(IsPresetActive(registry, enhanced));
}

TEST_CASE("the original preset turns smooth textures off and the enhanced one on") {
    settings::Registry registry = DeclaredRegistry();
    const std::vector<settings::SettingPreset> presets = EnhancementPresets();
    REQUIRE(presets.size() == 2);

    static_cast<void>(ApplyPreset(registry, presets[0]));
    CHECK_FALSE(registry.Get(kSmoothTexturesSetting));
    static_cast<void>(ApplyPreset(registry, presets[1]));
    CHECK(registry.Get(kSmoothTexturesSetting));
}

}
