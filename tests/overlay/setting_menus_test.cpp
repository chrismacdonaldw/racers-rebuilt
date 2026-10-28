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

std::vector<std::string_view> Categories(const std::vector<SettingMenu>& menus) {
    return menus | std::views::transform(&SettingMenu::category) | std::ranges::to<std::vector>();
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

TEST_CASE("settings without a category get no menu") {
    settings::Registry registry;
    registry.Declare(kHidden);

    CHECK(GroupSettingMenus(registry.AllDeclarations()).empty());
}

}
}
