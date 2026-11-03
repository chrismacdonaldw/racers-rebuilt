#include "settings/setting_preset.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace rr::settings {
namespace {

constexpr SettingSpec<bool> kRespawn{
    .key = "race.respawn", .defaultValue = true, .category = "Enhancements/Gameplay"};
constexpr SettingSpec<int> kPace{.key = "race.pace",
                                 .defaultValue = 100,
                                 .range = SettingRange<int>{.min = 50, .max = 150},
                                 .category = "Enhancements/Gameplay"};
constexpr SettingSpec<bool> kSibling{
    .key = "race.sibling", .defaultValue = false, .category = "Enhancements/Gameplay2"};
constexpr SettingSpec<bool> kCamera{
    .key = "camera.free", .defaultValue = false, .category = "Enhancements/Camera"};

Registry DeclaredRegistry() {
    Registry registry;
    registry.Declare(kRespawn);
    registry.Declare(kPace);
    registry.Declare(kSibling);
    registry.Declare(kCamera);
    return registry;
}

SettingPreset Fast() {
    return {.label = "Fast",
            .menu = "Enhancements",
            .resets = {"Enhancements/Gameplay"},
            .values = {{"race.pace", std::int64_t{400}}}};
}

}

TEST_CASE("a preset resets its categories, then applies its values") {
    Registry registry = DeclaredRegistry();
    registry.Set(kRespawn, false);
    registry.Set(kSibling, true);
    registry.Set(kCamera, true);

    const std::vector<std::string> changed = ApplyPreset(registry, Fast());

    CHECK(changed == std::vector<std::string>{"race.respawn", "race.pace"});
    CHECK(registry.Get(kRespawn));
    CHECK(registry.Get(kPace) == 150);
    CHECK(registry.Get(kSibling));
    CHECK(registry.Get(kCamera));
    CHECK(ApplyPreset(registry, Fast()).empty());
}

TEST_CASE("a preset is active while every setting it touches holds its value") {
    Registry registry = DeclaredRegistry();
    const SettingPreset defaults{.label = "Defaults", .resets = {"Enhancements/Gameplay"}};
    const SettingPreset slow{.label = "Slow",
                             .resets = {"Enhancements/Gameplay"},
                             .values = {{"race.pace", std::int64_t{60}}}};

    CHECK(IsPresetActive(registry, defaults));
    CHECK_FALSE(IsPresetActive(registry, slow));

    static_cast<void>(ApplyPreset(registry, slow));
    CHECK(IsPresetActive(registry, slow));
    CHECK_FALSE(IsPresetActive(registry, defaults));

    registry.Set(kRespawn, false);
    CHECK_FALSE(IsPresetActive(registry, slow));
    registry.Set(kSibling, true);
    registry.Set(kRespawn, true);
    CHECK(IsPresetActive(registry, slow));
}

TEST_CASE("a preset skips a value for a setting nobody declared") {
    Registry registry = DeclaredRegistry();
    const SettingPreset unknown{.label = "Unknown", .values = {{"race.missing", true}}};

    CHECK(ApplyPreset(registry, unknown).empty());
    CHECK_FALSE(registry.AllValues().contains("race.missing"));
}

}
