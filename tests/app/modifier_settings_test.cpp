#include "app/modifier_settings.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "app/cheat_settings.hpp"
#include "app/enhancement_presets.hpp"
#include "app/race_scene_name.hpp"
#include "app/race_settings.hpp"
#include "app/racer/racer_grid.hpp"
#include "settings/setting_preset.hpp"

namespace rr::app {
namespace {

settings::Registry DeclaredRegistry() {
    settings::Registry registry;
    DeclareRaceSettings(registry);
    DeclareCheatSettings(registry);
    DeclareModifierSettings(registry);
    return registry;
}

void TurnEverythingOn(settings::Registry& registry) {
    registry.Set(kGravityModifier, 25);
    registry.Set(kSpeedModifier, 200);
    registry.Set(kMirrorModifier, true);
    registry.Set(kOnlyPowerupModifier, std::string("warp"));
    registry.Set(kChaosBricksModifier, true);
    registry.Set(kSqueakyVoicesModifier, true);
    registry.Set(kRocketRushModifier, true);
}

}

TEST_CASE("every modifier is off by default") {
    const settings::Registry registry = DeclaredRegistry();

    CHECK(ModifiersFrom(registry) == RaceModifiers{});
}

TEST_CASE("the modifier settings carry into the race") {
    settings::Registry registry = DeclaredRegistry();
    TurnEverythingOn(registry);

    const RaceModifiers modifiers = ModifiersFrom(registry);

    CHECK(modifiers.rules.gravityScale == doctest::Approx(0.25F));
    CHECK(modifiers.rules.speedScale == doctest::Approx(2.0F));
    CHECK(modifiers.rules.onlyPowerup == sim::PowerupKind::Warp);
    CHECK(modifiers.rules.hasChaosBricks);
    CHECK(modifiers.mirrorsTracks);
    CHECK(modifiers.squeaksVoices);
    CHECK(modifiers.rushesRocketRacer);
}

TEST_CASE("each power-up choice names its kind in order") {
    settings::Registry registry = DeclaredRegistry();
    for (std::size_t kind = 0; kind < sim::kPowerupKindCount; ++kind) {
        registry.Set(kOnlyPowerupModifier, std::string(kOnlyPowerupChoices.at(kind + 1)));
        CHECK(ModifiersFrom(registry).rules.onlyPowerup == static_cast<sim::PowerupKind>(kind));
    }
    registry.Set(kOnlyPowerupModifier, std::string(kNoOnlyPowerup));
    CHECK_FALSE(ModifiersFrom(registry).rules.onlyPowerup.has_value());
}

TEST_CASE("the default preset turns every modifier off and the enhanced one keeps them") {
    settings::Registry registry = DeclaredRegistry();
    TurnEverythingOn(registry);
    const std::vector<settings::SettingPreset> presets = EnhancementPresets();
    REQUIRE(presets.size() == 2);

    static_cast<void>(ApplyPreset(registry, presets[1]));
    CHECK(ModifiersFrom(registry).mirrorsTracks);
    static_cast<void>(ApplyPreset(registry, presets[0]));
    CHECK(ModifiersFrom(registry) == RaceModifiers{});
}

TEST_CASE("the mirror modifier mirrors single races, circuits and time trials") {
    for (RaceSceneSetup setup :
         {RaceSceneSetup{.track = "RACEC0R0"}, RaceSceneSetup{.track = "RACEC0R0", .circuit = "c1"},
          RaceSceneSetup{.track = "RACEC0R0", .isTimeTrial = true}}) {
        CHECK_FALSE(LoadsMirrored(setup));
        setup.modifiers.mirrorsTracks = true;
        CHECK(LoadsMirrored(setup));
    }
}

TEST_CASE("Rocket Racer rush fills every computer racer's slot") {
    std::vector<std::string> codes{"CR", "KK", "BB", "JT", "GM", "BVB"};

    RushRocketRacer(codes, 1);

    CHECK(codes == std::vector<std::string>{"CR", "RR", "RR", "RR", "RR", "RR"});
    RushRocketRacer(codes, 9);
    CHECK(codes.front() == "CR");
}

}
