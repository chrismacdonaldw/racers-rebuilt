#include "app/player_cheat_settings.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "app/enhancement_presets.hpp"
#include "app/race_settings.hpp"
#include "settings/setting_preset.hpp"

namespace rr::app {
namespace {

settings::Registry DeclaredRegistry() {
    settings::Registry registry;
    DeclareRaceSettings(registry);
    DeclareCheatSettings(registry);
    DeclarePlayerCheatSettings(registry);
    return registry;
}

void TurnEverythingOn(settings::Registry& registry) {
    registry.Set(kInvincibleCheat, true);
    registry.Set(kBottomlessBrickCheat, true);
    registry.Set(kFullWhiteCheat, true);
    registry.Set(kBrickMagnetCheat, true);
    registry.Set(kLongTurboCheat, true);
    registry.Set(kPerfectStartCheat, true);
    registry.Set(kFreeWarpCheat, true);
    registry.Set(kHeadStartCheat, 4);
}

}

TEST_CASE("every player cheat is off by default") {
    const settings::Registry registry = DeclaredRegistry();

    CHECK(PlayerCheatsFrom(registry) == sim::PlayerCheats{});
}

TEST_CASE("the player cheat settings carry into the race") {
    settings::Registry registry = DeclaredRegistry();
    TurnEverythingOn(registry);

    const sim::PlayerCheats cheats = PlayerCheatsFrom(registry);

    CHECK(cheats.isInvincible);
    CHECK(cheats.hasBottomlessBrick);
    CHECK(cheats.keepsFullWhite);
    CHECK(cheats.hasBrickMagnet);
    CHECK(cheats.hasLongTurbo);
    CHECK(cheats.hasPerfectStart);
    CHECK(cheats.hasFreeWarp);
    CHECK(cheats.headStart == Milliseconds{4000});
}

TEST_CASE("the original preset turns every player cheat off and the enhanced one keeps them") {
    settings::Registry registry = DeclaredRegistry();
    TurnEverythingOn(registry);
    const std::vector<settings::SettingPreset> presets = EnhancementPresets();
    REQUIRE(presets.size() == 2);

    static_cast<void>(ApplyPreset(registry, presets[1]));
    CHECK(PlayerCheatsFrom(registry).isInvincible);
    static_cast<void>(ApplyPreset(registry, presets[0]));
    CHECK(PlayerCheatsFrom(registry) == sim::PlayerCheats{});
}

}
