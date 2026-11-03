#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "overlay/headless_ui.hpp"
#include "overlay/setting_menus.hpp"

namespace rr::overlay {
namespace {

using settings::SettingSpec;

constexpr SettingSpec<int> kFpsCap{.key = "video.fps_cap",
                                   .defaultValue = 0,
                                   .range = settings::SettingRange<int>{.min = 0, .max = 360},
                                   .label = "Framerate cap",
                                   .category = "Video"};

TEST_CASE("dragging a slider changes the setting at once and finishes on release") {
    settings::Registry registry;
    registry.Declare(kFpsCap);
    const SettingMenu menu = GroupSettingMenus(registry.AllDeclarations()).front();
    HeadlessUi ui;
    CHECK(ui.Frame(registry, menu).empty());

    ui.MoveMouse(0.01F);
    CHECK(ui.Frame(registry, menu).empty());
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    CHECK(ui.Frame(registry, menu).empty());
    ui.MoveMouse(0.5F);
    CHECK(ui.Frame(registry, menu).empty());

    CHECK(registry.Get(kFpsCap) > 100);
    CHECK(registry.Get(kFpsCap) < 260);
    ImGui::GetIO().AddMouseButtonEvent(0, false);
    CHECK(ui.Frame(registry, menu) == std::vector<std::string>{"video.fps_cap"});
}

TEST_CASE("a grid cell edits its setting and finishes on enter") {
    settings::Registry registry;
    constexpr SettingSpec<std::string_view> kDrift{
        .key = "input.keys.drift", .defaultValue = "", .label = "Drift", .category = "B/Keys"};
    registry.Declare(kDrift);
    const SettingMenu menu = GroupSettingMenus(registry.AllDeclarations()).front();
    HeadlessUi ui;
    CHECK(ui.Frame(registry, menu, DrawSettingGrid).empty());
    CHECK(ui.Frame(registry, menu, DrawSettingGrid).empty());

    ui.MoveMouse(0.9F, 0.8F);
    CHECK(ui.Frame(registry, menu, DrawSettingGrid).empty());
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    CHECK(ui.Frame(registry, menu, DrawSettingGrid).empty());
    ImGui::GetIO().AddMouseButtonEvent(0, false);
    CHECK(ui.Frame(registry, menu, DrawSettingGrid).empty());
    ImGui::GetIO().AddInputCharactersUTF8("Q");
    CHECK(ui.Frame(registry, menu, DrawSettingGrid).empty());
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true);

    CHECK(ui.Frame(registry, menu, DrawSettingGrid) ==
          std::vector<std::string>{"input.keys.drift"});
    CHECK(registry.Get(kDrift) == "Q");
}

}
}
