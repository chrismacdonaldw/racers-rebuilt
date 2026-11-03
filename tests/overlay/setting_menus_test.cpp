#include "overlay/setting_menus.hpp"

#include <doctest/doctest.h>

#include <ranges>
#include <string_view>
#include <vector>

namespace rr::overlay {
namespace {

using settings::SettingSpec;

constexpr SettingSpec<bool> kVsync{.key = "video.vsync", .defaultValue = true, .category = "Video"};
constexpr SettingSpec<int> kFpsCap{.key = "video.fps_cap", .defaultValue = 60, .category = "Video"};
constexpr SettingSpec<double> kVolume{
    .key = "audio.music_volume", .defaultValue = 1.0, .category = "Audio"};
constexpr SettingSpec<std::string_view> kHidden{.key = "internal.note", .defaultValue = ""};

template <typename Menu>
std::vector<std::string_view> Categories(const std::vector<Menu>& menus) {
    return menus | std::views::transform(&Menu::category) | std::ranges::to<std::vector>();
}

TEST_CASE("settings are grouped into menus by category, sorted by category and key") {
    settings::Registry registry;
    registry.Declare(kVsync);
    registry.Declare(kVolume);
    registry.Declare(kFpsCap);

    const std::vector<SettingMenu> menus = GroupSettingMenus(registry.AllDeclarations());

    REQUIRE(Categories(menus) == std::vector<std::string_view>{"Audio", "Video"});
    CHECK(menus[0].keys == std::vector<std::string_view>{"audio.music_volume"});
    CHECK(menus[1].keys == std::vector<std::string_view>{"video.fps_cap", "video.vsync"});
}

TEST_CASE("a newly declared setting gets a menu with no other code") {
    settings::Registry registry;
    registry.Declare(kVsync);
    constexpr SettingSpec<bool> kNew{
        .key = "hud.speedometer", .defaultValue = false, .label = "Speedometer", .category = "HUD"};
    registry.Declare(kNew);

    const std::vector<SettingMenu> menus = GroupSettingMenus(registry.AllDeclarations());

    REQUIRE(Categories(menus) == std::vector<std::string_view>{"HUD", "Video"});
    CHECK(menus[0].keys == std::vector<std::string_view>{"hud.speedometer"});
}

TEST_CASE("a slash in a category makes a submenu, sorted by name") {
    settings::Registry registry;
    constexpr SettingSpec<bool> kFree{
        .key = "enhancements.free", .defaultValue = false, .category = "Enhancements"};
    constexpr SettingSpec<bool> kRespawn{
        .key = "race.respawn", .defaultValue = true, .category = "Enhancements/Gameplay"};
    constexpr SettingSpec<bool> kCamera{
        .key = "camera.free", .defaultValue = false, .category = "Enhancements/Camera"};
    registry.Declare(kFree);
    registry.Declare(kRespawn);
    registry.Declare(kCamera);

    const std::vector<SettingMenu> menus = GroupSettingMenus(registry.AllDeclarations());

    REQUIRE(Categories(menus) == std::vector<std::string_view>{"Enhancements"});
    CHECK(menus[0].keys == std::vector<std::string_view>{"enhancements.free"});
    REQUIRE(Categories(menus[0].submenus) == std::vector<std::string_view>{"Camera", "Gameplay"});
    CHECK(menus[0].submenus[0].keys == std::vector<std::string_view>{"camera.free"});
    CHECK(menus[0].submenus[1].keys == std::vector<std::string_view>{"race.respawn"});
}

TEST_CASE("settings without a category get no menu") {
    settings::Registry registry;
    registry.Declare(kHidden);

    CHECK(GroupSettingMenus(registry.AllDeclarations()).empty());
}

TEST_CASE("a grid has a column per submenu and a row per label, blank where a column lacks it") {
    settings::Registry registry;
    constexpr SettingSpec<std::string_view> kKeysBrake{
        .key = "input.keys.brake", .defaultValue = "Down", .label = "Brake", .category = "B/Keys"};
    constexpr SettingSpec<std::string_view> kKeysBack{.key = "input.keys.menu_back",
                                                      .defaultValue = "Escape",
                                                      .label = "Menu back",
                                                      .category = "B/Keys"};
    constexpr SettingSpec<std::string_view> kPadBrake{
        .key = "input.pad.brake", .defaultValue = "b", .label = "Brake", .category = "B/Pad"};
    constexpr SettingSpec<std::string_view> kPadAccelerate{.key = "input.pad.accelerate",
                                                           .defaultValue = "a",
                                                           .label = "Accelerate",
                                                           .category = "B/Pad"};
    registry.Declare(kKeysBrake);
    registry.Declare(kKeysBack);
    registry.Declare(kPadBrake);
    registry.Declare(kPadAccelerate);
    const SettingMenu menu = GroupSettingMenus(registry.AllDeclarations()).front();

    const SettingGrid grid = GroupSettingGrid(registry.AllDeclarations(), menu);

    CHECK(grid.columns == std::vector<std::string_view>{"Keys", "Pad"});
    REQUIRE(grid.rows.size() == 3);
    CHECK(grid.rows[0].label == "Accelerate");
    CHECK(grid.rows[0].keys == std::vector<std::string_view>{"", "input.pad.accelerate"});
    CHECK(grid.rows[1].label == "Brake");
    CHECK(grid.rows[1].keys ==
          std::vector<std::string_view>{"input.keys.brake", "input.pad.brake"});
    CHECK(grid.rows[2].label == "Menu back");
    CHECK(grid.rows[2].keys == std::vector<std::string_view>{"input.keys.menu_back", ""});
}

}
}
