#include "settings/quick_config.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "temp_folder.hpp"

namespace rr::settings {
namespace {

constexpr SettingSpec<bool> kRespawn{
    .key = "race.respawn", .defaultValue = false, .category = "Enhancements/Gameplay"};
constexpr SettingSpec<int> kPace{.key = "race.pace",
                                 .defaultValue = 100,
                                 .range = SettingRange<int>{.min = 50, .max = 150},
                                 .category = "Enhancements/Gameplay"};
constexpr SettingSpec<bool> kSmooth{
    .key = "video.smooth", .defaultValue = false, .category = "Video"};
constexpr SettingSpec<double> kScale{
    .key = "video.scale", .defaultValue = 1.0, .category = "Video"};
constexpr SettingSpec<bool> kPack{
    .key = "mods.pack.enabled", .defaultValue = true, .category = "Mods"};
constexpr SettingSpec<double> kVolume{
    .key = "audio.volume", .defaultValue = 1.0, .category = "Audio"};

Registry DeclaredRegistry() {
    Registry registry;
    registry.Declare(kRespawn);
    registry.Declare(kPace);
    registry.Declare(kSmooth);
    registry.Declare(kScale);
    registry.Declare(kPack);
    registry.Declare(kVolume);
    return registry;
}

std::vector<SettingPreset> Shipped() {
    return {{.label = "Default", .menu = "Enhancements", .resets = {"Enhancements"}},
            {.label = "Enhanced",
             .menu = "Enhancements",
             .resets = {"Enhancements/Gameplay"},
             .values = {{"video.smooth", true}, {"video.scale", 2.0}}}};
}

void WriteText(const std::filesystem::path& path, std::string_view text) {
    std::ofstream(path, std::ios::binary) << text;
}

}

TEST_CASE("a quick config slug keeps lowercase letters and digits") {
    CHECK(QuickConfigSlug("Race Night 2") == "race-night-2");
    CHECK(QuickConfigSlug("  --Fast!!  ") == "fast");
    CHECK(QuickConfigSlug("???") == "config");
}

TEST_CASE("the quick config scope is the shipped resets, their named keys and the mods") {
    const Registry registry = DeclaredRegistry();
    CHECK(QuickConfigScope(registry, Shipped()) ==
          std::vector<std::string>{"mods.pack.enabled", "race.pace", "race.respawn", "video.scale",
                                   "video.smooth"});
}

TEST_CASE("a quick config round trips through its file") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "night.toml";
    const QuickConfig config{
        .name = "Race night",
        .values = {{"race.pace", std::int64_t{120}}, {"race.respawn", true}, {"video.scale", 1.5}}};

    REQUIRE(WriteQuickConfig(config, path));
    const Result<QuickConfig> read = ReadQuickConfig(path);

    REQUIRE(read);
    CHECK(read->name == "Race night");
    CHECK(read->values == config.values);
}

TEST_CASE("a quick config reads quoted keys and takes its file's name when it has none") {
    const test::TempFolder folder;
    const std::filesystem::path path = folder.Path() / "fast.toml";
    WriteText(path, "[settings]\n\"race.respawn\" = true\n[settings.race]\npace = 90\n");

    const Result<QuickConfig> read = ReadQuickConfig(path);

    REQUIRE(read);
    CHECK(read->name == "fast");
    const std::vector<std::pair<std::string, SettingValue>> expected{
        {"race.pace", std::int64_t{90}}, {"race.respawn", true}};
    CHECK(read->values == expected);
    CHECK(ReadQuickConfig(folder.Path() / "missing.toml")->values.empty());
}

TEST_CASE("a snapshot applied later restores every setting in scope") {
    Registry registry = DeclaredRegistry();
    const std::vector<std::string> scope = QuickConfigScope(registry, Shipped());
    registry.Set(kRespawn, true);
    registry.Set(kPace, 130);
    registry.Set(kPack, false);
    registry.Set(kVolume, 0.5);
    const SettingPreset preset = QuickConfigPreset(SnapshotQuickConfig(registry, scope, "Mine"),
                                                   registry, scope, "Enhancements");
    registry.Set(kRespawn, false);
    registry.Set(kPace, 60);
    registry.Set(kPack, true);
    registry.Set(kVolume, 0.25);
    CHECK_FALSE(IsPresetActive(registry, preset));

    static_cast<void>(ApplyPreset(registry, preset));

    CHECK(preset.label == "Mine");
    CHECK(registry.Get(kRespawn));
    CHECK(registry.Get(kPace) == 130);
    CHECK_FALSE(registry.Get(kPack));
    CHECK(registry.Get(kVolume) == doctest::Approx(0.25));
    CHECK(IsPresetActive(registry, preset));
}

TEST_CASE("a quick config defaults what it lacks and fits what it holds") {
    Registry registry = DeclaredRegistry();
    const std::vector<std::string> scope = QuickConfigScope(registry, Shipped());
    registry.Set(kSmooth, true);
    const QuickConfig config{.name = "Odd",
                             .values = {{"race.pace", std::int64_t{400}},
                                        {"race.respawn", std::int64_t{1}},
                                        {"video.scale", std::int64_t{2}},
                                        {"audio.volume", 0.1},
                                        {"race.unknown", true}}};

    const SettingPreset preset = QuickConfigPreset(config, registry, scope, "Enhancements");
    static_cast<void>(ApplyPreset(registry, preset));

    CHECK(registry.Get(kPace) == 150);
    CHECK_FALSE(registry.Get(kRespawn));
    CHECK(registry.Get(kScale) == doctest::Approx(2.0));
    CHECK_FALSE(registry.Get(kSmooth));
    CHECK(registry.Get(kVolume) == doctest::Approx(1.0));
    CHECK(IsPresetActive(registry, preset));
}

}
