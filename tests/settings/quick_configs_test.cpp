#include "settings/quick_configs.hpp"

#include <doctest/doctest.h>

#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "temp_folder.hpp"

namespace rr::settings {
namespace {

constexpr SettingSpec<bool> kRespawn{
    .key = "race.respawn", .defaultValue = false, .category = "Enhancements/Gameplay"};
constexpr SettingSpec<bool> kPack{
    .key = "mods.pack.enabled", .defaultValue = true, .category = "Mods"};

Registry DeclaredRegistry() {
    Registry registry;
    registry.Declare(kRespawn);
    registry.Declare(kPack);
    return registry;
}

std::vector<SettingPreset> Shipped() {
    return {{.label = "Default", .menu = "Enhancements", .resets = {"Enhancements"}}};
}

std::vector<std::string> Labels(const QuickConfigs& configs) {
    std::vector<std::string> labels;
    for (const UserQuickConfig& config : configs.User()) {
        labels.push_back(config.preset.label);
    }
    return labels;
}

void WriteText(const std::filesystem::path& path, std::string_view text) {
    std::ofstream(path, std::ios::binary) << text;
}

}

TEST_CASE("quick configs list the folder's files by name and skip broken ones") {
    const test::TempFolder folder;
    WriteText(folder.Path() / "zed.toml", "name = \"Alpha\"\n");
    WriteText(folder.Path() / "beta.toml",
              "name = \"Beta\"\n[settings]\n\"race.respawn\" = true\n");
    WriteText(folder.Path() / "broken.toml", "name = \n");
    WriteText(folder.Path() / "notes.txt", "name = \"Notes\"\n");
    const Registry registry = DeclaredRegistry();
    QuickConfigs configs(Shipped(), folder.Path());

    configs.Reload(registry);

    CHECK(Labels(configs) == std::vector<std::string>{"Alpha", "Beta"});
    CHECK(configs.User()[0].slug == "zed");
    CHECK(configs.User()[0].preset.menu == "Enhancements");
    REQUIRE(configs.Find("beta") != nullptr);
    CHECK(configs.Find("Beta")->preset.label == "Beta");
    CHECK(configs.Find("gamma") == nullptr);
}

TEST_CASE("saving over a hand-named file finds it by its slug and keeps its file") {
    const test::TempFolder folder;
    WriteText(folder.Path() / "My Pack.toml", "name = \"My Pack\"\n");
    Registry registry = DeclaredRegistry();
    QuickConfigs configs(Shipped(), folder.Path());
    configs.Reload(registry);
    REQUIRE(configs.Find("my pack") != nullptr);

    registry.Set(kRespawn, true);
    REQUIRE(configs.Save(registry, "My Pack"));

    CHECK(Labels(configs) == std::vector<std::string>{"My Pack"});
    CHECK_FALSE(std::filesystem::exists(folder.Path() / "my-pack.toml"));
}

TEST_CASE("a missing quick config folder lists nothing") {
    const test::TempFolder folder;
    QuickConfigs configs(Shipped(), folder.Path() / "presets");

    configs.Reload(DeclaredRegistry());

    CHECK(configs.User().empty());
    CHECK(configs.Shipped().size() == 1);
}

TEST_CASE("saving a quick config writes it, replaces its slug, and removing deletes it") {
    const test::TempFolder folder;
    Registry registry = DeclaredRegistry();
    QuickConfigs configs(Shipped(), folder.Path() / "presets");
    registry.Set(kRespawn, true);
    registry.Set(kPack, false);

    REQUIRE(configs.Save(registry, "Race Night"));
    REQUIRE(configs.Save(registry, "race night"));

    CHECK(Labels(configs) == std::vector<std::string>{"race night"});
    CHECK(std::filesystem::exists(folder.Path() / "presets" / "race-night.toml"));
    registry.Set(kRespawn, false);
    registry.Set(kPack, true);
    static_cast<void>(ApplyPreset(registry, configs.User()[0].preset));
    CHECK(registry.Get(kRespawn));
    CHECK_FALSE(registry.Get(kPack));

    REQUIRE(configs.Remove(registry, "race-night"));
    CHECK(configs.User().empty());
    CHECK_FALSE(std::filesystem::exists(folder.Path() / "presets" / "race-night.toml"));
}

}
